#!/usr/bin/env python3
"""Deploy/update the reusable Listen app. Ordinary audio uploads use publish.py."""
from datetime import datetime, timezone
import json
from pathlib import Path
import shlex
import subprocess

HERE = Path(__file__).resolve().parent
TARGET = json.loads((HERE / 'target.json').read_text())


def ssh(command, *args, input=None):
    return subprocess.run(['ssh', '-o', 'BatchMode=yes', TARGET['ssh'], ' '.join(shlex.quote(str(v)) for v in [command, *args])], input=input, text=True, check=True)


def main():
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    root = TARGET['root']; release = root + '/releases/' + stamp
    ssh('mkdir', '-p', release + '/public', root + '/data/collections', root + '/bin')
    transport = ['rsync', '-a', '-e', 'ssh -o BatchMode=yes']
    subprocess.run([*transport, str(HERE / 'server.mjs'), f'{TARGET["ssh"]}:{release}/server.mjs'], check=True)
    subprocess.run([*transport, str(HERE / 'server.test.mjs'), f'{TARGET["ssh"]}:{release}/server.test.mjs'], check=True)
    subprocess.run([*transport, str(HERE / 'public') + '/', f'{TARGET["ssh"]}:{release}/public/'], check=True)
    subprocess.run([*transport, str(HERE / 'ensure_proxy.py'), f'{TARGET["ssh"]}:{root}/bin/ensure_proxy.py'], check=True)
    setup = '''import pathlib,secrets,os,sys,json,subprocess,time
root=pathlib.Path(sys.argv[1]);release=sys.argv[2];target=json.loads(sys.argv[3]);env=root/'server.env'
if not env.exists():
 fd=os.open(env,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
 with os.fdopen(fd,'w') as f:f.write('ACCESS_KEY='+secrets.token_urlsafe(36)+'\\n')
os.chmod(env,0o600)
name=target['appContainer'];old=None
inspection=subprocess.run(['docker','inspect',name],capture_output=True,text=True)
if inspection.returncode==0:
 info=json.loads(inspection.stdout)[0]
 assert info['Config'].get('Labels',{}).get('com.nk.listen')=='1','Existing container is not owned by Listen'
 old=name+'-previous-'+str(time.time_ns())
 subprocess.run(['docker','stop',name],check=True,stdout=subprocess.DEVNULL)
 subprocess.run(['docker','rename',name,old],check=True)
command=['docker','run','-d','--name',name,'--label','com.nk.listen=1','--restart','unless-stopped','--network',target['dockerNetwork'],
 '--user','node','--read-only','--tmpfs','/tmp:size=16m','--cap-drop','ALL','--security-opt','no-new-privileges:true',
 '--memory','256m','--pids-limit','64','--log-opt','max-size=10m','--log-opt','max-file=3','--env-file',str(env),
 '-e','HOST=0.0.0.0','-e','PORT=3048','-e','BASE_PATH=/listen','-e','DATA_ROOT=/data','-e','SECURE_COOKIES=true',
 '-e','PUBLIC_ORIGIN='+target['url'].split('/listen/')[0],'-e','NODE_ENV=production',
 '-v',release+':/app:ro','-v',str(root/'data')+':/data:ro',target['image'],'node','/app/server.mjs']
try:
 subprocess.run(command,check=True,stdout=subprocess.DEVNULL)
 healthy=False
 for i in range(20):
  result=subprocess.run(['docker','exec',name,'node','-e',"fetch('http://127.0.0.1:3048/listen/health').then(r=>process.exit(r.ok?0:1)).catch(()=>process.exit(1))"],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  if result.returncode==0:healthy=True;break
  time.sleep(.5)
 assert healthy,'New Listen app failed health check'
 subprocess.run(['docker','exec',name,'node','--test','/app/server.test.mjs'],check=True)
except Exception:
 subprocess.run(['docker','rm','-f',name],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
 if old:
  subprocess.run(['docker','rename',old,name],check=True);subprocess.run(['docker','start',name],check=True,stdout=subprocess.DEVNULL)
 raise
(root/'deployment.json').write_text(json.dumps(dict(release=release,container=name,previous_container=old,image=target['image'],url=target['url']),indent=2)+'\\n')
print('Listen app healthy:',target['url'])
'''
    ssh('python3', '-c', setup, root, release, json.dumps(TARGET))
    ssh('python3', root + '/bin/ensure_proxy.py', '--proxy', TARGET['proxyContainer'], '--app', TARGET['appContainer'], '--root', root)
    print('Deployed:', TARGET['url'])


if __name__ == '__main__': main()
