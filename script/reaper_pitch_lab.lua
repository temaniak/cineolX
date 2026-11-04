-- Cineol Pitch Lab: local, file-based instrumentation through official ReaScript.
-- Load this action in an EMPTY NEW PROJECT TAB, or the saved bench project.
-- No arbitrary Lua commands, network listener, user-project access or audio-thread I/O.
local _, script_path = reaper.get_action_context()
script_path = script_path:gsub("\\", "/")
local root = assert(script_path:match("^(.*)/script/[^/]+$"), "Place this script in the repository script folder")
local work = root .. "/build/pitch-lab/reaper-bench"
local project_file = work .. "/Cineol-Pitch-Lab-Bench.RPP"
reaper.RecursiveCreateDirectory(work .. "/renders", 0)
local project, filename = reaper.EnumProjects(-1, "")
if filename ~= project_file and (filename ~= "" or reaper.CountTracks(project) > 0) then
    reaper.ShowMessageBox("Create an empty NEW PROJECT TAB before running Pitch Lab. Existing projects are never repurposed.", "Cineol Pitch Lab", 0)
    return
end
local function json(v)
    local t=type(v)
    if t=="nil" then return "null" end
    if t=="boolean" then return v and "true" or "false" end
    if t=="number" then return (v==v and math.abs(v)<math.huge) and string.format("%.12g",v) or "null" end
    if t=="string" then return '"'..v:gsub('\\','\\\\'):gsub('"','\\"'):gsub('\n','\\n'):gsub('\r','\\r'):gsub('\t','\\t')..'"' end
    if t=="table" then
        local a={}
        if #v>0 then for _,x in ipairs(v) do a[#a+1]=json(x) end;return '['..table.concat(a,',')..']' end
        for k,x in pairs(v) do a[#a+1]=json(tostring(k))..':'..json(x) end
        return '{'..table.concat(a,',')..'}'
    end
    return json(tostring(v))
end
local function write_json(path,value)
    local temp=path..".tmp"
    local f=assert(io.open(temp,"wb"));f:write(json(value));f:close();assert(os.rename(temp,path))
end
local function log(message)
    local f=io.open(work.."/bridge.log","a");if f then f:write(os.date("!%Y-%m-%dT%H:%M:%SZ")," ",message,"\n");f:close() end
end
if reaper.CountTracks(project)==0 then
    reaper.InsertTrackAtIndex(0,true)
    local tr=reaper.GetTrack(project,0)
    reaper.GetSetMediaTrackInfo_String(tr,"P_NAME","Pitch Lab / instrumented DUT",true)
end
local track=reaper.GetTrack(project,0)
reaper.SetMediaTrackInfo_Value(track,"I_RECARM",0)
local master=reaper.GetMasterTrack(project)
reaper.SetMediaTrackInfo_Value(master,"B_MUTE",1)
reaper.GetSetProjectInfo(project,"PROJECT_SRATE",48000,true)
reaper.GetSetProjectInfo(project,"PROJECT_SRATE_USE",1,true)
reaper.GetSetProjectInfo_String(project,"PROJECT_TITLE","Cineol Pitch Lab / isolated instrumented bench",true)
reaper.Main_SaveProjectEx(project,project_file,8)
local fx=-1
for n=0,reaper.TrackFX_GetCount(track)-1 do
    local _,name=reaper.TrackFX_GetFXName(track,n,"")
    if name:find("Cineol Pitch Lab",1,true) then fx=n;break end
end
if fx<0 then fx=reaper.TrackFX_AddByName(track,"VST3: Cineol Pitch Lab (NK Stuff)",false,-1) end
if fx<0 then fx=reaper.TrackFX_AddByName(track,"AU: NK Stuff: Cineol Pitch Lab",false,-1) end
if fx<0 then
    write_json(work.."/status.json",{ok=false,error="Cineol Pitch Lab not discovered. Scan for new plugins, then run this action again."})
    reaper.ShowMessageBox("Cineol Pitch Lab is not in REAPER's FX list yet. Scan for new plugins and run this action again. The bench project has been saved separately.","Pitch Lab",0)
    return
end
local function validate()
    assert(reaper.ValidatePtr2(project,track,"MediaTrack*"),"Bench project has closed")
    local _,path=reaper.EnumProjects(-1,"")
    local _,name=reaper.TrackFX_GetFXName(track,fx,"")
    assert(name:find("Cineol Pitch Lab",1,true),"Bench FX identity changed")
    return path==project_file
end
local function param_index(name)
    for i=0,reaper.TrackFX_GetNumParams(track,fx)-1 do
        local _,n=reaper.TrackFX_GetParamName(track,fx,i,"")
        if n==name then return i end
    end
    error("Unknown Pitch Lab parameter: "..name)
end
local function panic()
    local i=param_index("Reset Histories Trigger")
    reaper.TrackFX_SetParamNormalized(track,fx,i,reaper.TrackFX_GetParamNormalized(track,fx,i)>.5 and 0 or 1)
end
local function use_stimulus(path)
    assert(path:sub(1,#work+1)==work.."/" and not path:find("%.%."),"Stimulus must be inside the bench folder")
    local src=assert(reaper.PCM_Source_CreateFromFile(path),"Cannot open stimulus WAV")
    local item=reaper.GetTrackMediaItem(track,0)
    if not item then item=reaper.AddMediaItemToTrack(track) end
    local take=reaper.GetActiveTake(item)
    if not take then take=reaper.AddTakeToMediaItem(item) end
    reaper.SetMediaItemTake_Source(take,src)
    local length=reaper.GetMediaSourceLength(src)
    reaper.SetMediaItemInfo_Value(item,"D_POSITION",0)
    reaper.SetMediaItemInfo_Value(item,"D_LENGTH",length)
    reaper.SetMediaItemTakeInfo_Value(take,"D_STARTOFFS",0)
    reaper.SetMediaItemInfo_Value(item,"D_FADEINLEN",0)
    reaper.SetMediaItemInfo_Value(item,"D_FADEOUTLEN",0)
    reaper.UpdateItemInProject(item)
end
local f=io.open(work.."/stimulus.wav","rb");if f then f:close();use_stimulus(work.."/stimulus.wav") end
reaper.Main_SaveProjectEx(project,project_file,8)
local function status()
    local _,name=reaper.TrackFX_GetFXName(track,fx,"")
    local params={}
    for i=0,reaper.TrackFX_GetNumParams(track,fx)-1 do
        local _,n=reaper.TrackFX_GetParamName(track,fx,i,"")
        local _,text=reaper.TrackFX_GetFormattedParamValue(track,fx,i,"")
        local _,ident=reaper.TrackFX_GetParamIdent(track,fx,i,"")
        params[#params+1]={index=i,name=n,ident=ident,value=reaper.TrackFX_GetParamNormalized(track,fx,i),text=text}
    end
    local _,format=reaper.GetSetProjectInfo_String(project,"RENDER_FORMAT","",false)
    return {ok=true,version=reaper.GetAppVersion(),project=project_file,fx=name,fx_index=fx,parameters=params,
        peak_l=reaper.Track_GetPeakInfo(track,0),peak_r=reaper.Track_GetPeakInfo(track,1),playing=reaper.GetPlayStateEx(project),render_format=format,
        master_muted=(reaper.GetMasterMuteSoloFlags() & 1)~=0,heartbeat=reaper.time_precise()}
end
local last_id,last_poll,running="",0,true
-- Restarting the bridge must never replay a previous render, transport or close request.
do
    local pending=io.open(work.."/request.tsv","r")
    if pending then last_id=pending:read("*a"):match("request_id\t([^\r\n]+)") or "";pending:close() end
end
local function dispatch(fields,rows)
    assert(validate(),"Activate the isolated Pitch Lab project before issuing commands")
    local action=fields.action
    if action=="status" then return status()
    elseif action=="set" then
        if #rows>1 then reaper.Main_OnCommandEx(1016,0,project) end
        for _,row in ipairs(rows) do
            local value=assert(tonumber(row[3]),"Invalid normalized value")
            assert(value>=0 and value<=1,"Normalized parameter outside 0..1")
            reaper.TrackFX_SetParamNormalized(track,fx,param_index(row[2]),value)
        end
        if fields.reset=="1" then panic() end
        return status()
    elseif action=="panic" then panic();return status()
    elseif action=="stimulus" then
        reaper.Main_OnCommandEx(1016,0,project);use_stimulus(assert(fields.path));panic();return status()
    elseif action=="save" then reaper.Main_SaveProjectEx(project,project_file,8);return status()
    elseif action=="play" then
        reaper.SetMediaTrackInfo_Value(master,"B_MUTE",1);reaper.SetEditCurPos2(project,0,false,false);reaper.Main_OnCommandEx(1007,0,project);return status()
    elseif action=="stop" then reaper.Main_OnCommandEx(1016,0,project);return status()
    elseif action=="close_bridge" then running=false;return {ok=true,stopped=true}
    elseif action=="render" then
        assert(reaper.CountTracks(project)==1,"The instrumented bench must contain exactly one DUT track")
        local name=assert(fields.name);assert(name:match("^[%w_%-]+$"),"Use a simple unique render name")
        local seconds=tonumber(fields.seconds) or 8;assert(seconds>0 and seconds<=120,"Invalid render length")
        local destination=work.."/renders/"..name..".wav"
        local existing=io.open(destination,"rb");if existing then existing:close();error("Render already exists; choose a new name") end
        reaper.Main_OnCommandEx(1016,0,project);reaper.SetEditCurPos2(project,0,false,false)
        reaper.TrackFX_SetOffline(track,fx,true);reaper.TrackFX_SetOffline(track,fx,false)
        for k,v in pairs({RENDER_SETTINGS=0,RENDER_BOUNDSFLAG=0,RENDER_STARTPOS=0,RENDER_ENDPOS=seconds,
            RENDER_SRATE=48000,RENDER_CHANNELS=2,RENDER_DITHER=0,RENDER_TAILFLAG=0,RENDER_ADDTOPROJ=0,RENDER_NORMALIZE=0}) do
            reaper.GetSetProjectInfo(project,k,v,true)
        end
        reaper.GetSetProjectInfo_String(project,"RENDER_FILE",work.."/renders",true)
        reaper.GetSetProjectInfo_String(project,"RENDER_PATTERN",name,true)
        local _,format=reaper.GetSetProjectInfo_String(project,"RENDER_FORMAT","",false)
        if format=="" then reaper.GetSetProjectInfo_String(project,"RENDER_FORMAT","evaw",true) end
        reaper.SetMediaTrackInfo_Value(master,"B_MUTE",0)
        -- Native action: render using the last settings. Enable auto-close in Render to File.
        local rendered,render_error=pcall(reaper.Main_OnCommandEx,41824,0,project)
        reaper.SetMediaTrackInfo_Value(master,"B_MUTE",1)
        assert(rendered,render_error)
        local _,stats=reaper.GetSetProjectInfo_String(project,"RENDER_STATS","",false)
        reaper.Main_SaveProjectEx(project,project_file,8)
        return {ok=true,path=destination,stats=stats,status=status()}
    end
    error("Unsupported command: "..tostring(action))
end
local function loop()
    local now=reaper.time_precise()
    if now-last_poll>.15 then
        last_poll=now
        local file=io.open(work.."/request.tsv","r")
        if file then
            local fields,rows={},{}
            for line in file:lines() do
                local key,a,b=line:match("^([^\t]+)\t([^\t]*)\t?(.*)$")
                if key=="param" then rows[#rows+1]={key,a,b} elseif key then fields[key]=a end
            end
            file:close()
            if fields.request_id and fields.request_id~=last_id then
                last_id=fields.request_id
                local ok,result=pcall(dispatch,fields,rows)
                if not ok then result={ok=false,error=tostring(result)} end
                result.request_id=last_id;write_json(work.."/response.json",result);log(fields.action.." "..tostring(result.ok))
            end
        end
    end
    if running and reaper.ValidatePtr2(project,track,"MediaTrack*") then reaper.defer(loop) end
end
reaper.atexit(function()if reaper.ValidatePtr2(project,master,"MediaTrack*") then reaper.SetMediaTrackInfo_Value(master,"B_MUTE",1) end;log("bridge stopped")end)
write_json(work.."/status.json",status());log("bridge ready");loop()
