// Physical program selection, followed by a labelled offline FPC/WCS clone.
// No converter phase or DSP state is injected into the native audio runtime.
#include "xl_reference.hpp"
#include <emulator/timing.hpp>
#include <array>
#include <fstream>
#include <memory>
using namespace lexplug;using namespace lexplug::op;using xl_test::require;
using namespace cineol::xl;

int main(int argc,char** argv) {
    require(argc==3,"usage: cineol_xl_converter_phase_check ROM_DIRECTORY OUTPUT_CSV");
    std::ofstream output(argv[2]);require(bool(output),"cannot write XL converter phase CSV");
    output<<"program,rows,adc_left_read,adc_right_read,left_hold_row,right_hold_row,request_row,channel_mask,capture_row,adc_observations,dac_events,dac_channel_observations\n";
    auto engine=std::make_unique<Engine>(0);xl_test::load(*engine,argv[1]);
    auto machine=std::make_unique<Machine>(*engine);LarcOperator op(*machine);
    require(!machine->run_task([&]{return xl_test::boot(*machine,op);}).failed,"XL converter boot failed");
    unsigned total_adc=0,total_dac=0;
    for(unsigned program=0;program<graphs.size();++program) {
        const auto info=graphs[program];
        require(!machine->run_task([&]{return xl_test::select(*machine,op,info.bank,info.program);}).failed,"XL converter selection failed");
        const auto& source=*engine->host().dsp;
        xl_test::ShapeCheck shape{source,Graph(program)};shape.run();require(shape.valid,"unsupported XL converter graph shape");
        auto clone=std::make_unique<lexicon224x::Machine>();
        std::copy(std::begin(source.wcs),std::end(source.wcs),std::begin(clone->wcs));
        clone->adc_left=100;clone->adc_right=200;clone->gain_left=clone->gain_right=0;
        std::array<int,2> reads{-1,-1},holds{999,999};std::array<int,128> captures{};captures.fill(-1);
        std::array<unsigned,128> request_masks{};
        for(unsigned r=0;r<info.rows;++r) {
            const auto mi=lexicon224x::decode(source.wcs[r]);
            require(!mi.wr_da || mi.channels,"zero-mask WR_DA needs explicit native graph metadata");
            if(mi.wr_da)request_masks[r]=mi.channels;
        }
        std::array<int64_t,2> loads{-1,-1};int64_t waiting=-1,converting=-1;
        unsigned adc_count=0,dac_count=0,dac_events=0,adc_slot=0;
        constexpr unsigned warm_passes=12,observed_passes=9;
        for(unsigned row=0;row<info.rows*(warm_passes+observed_passes);++row) {
            const unsigned local=row%info.rows;if(!local) adc_slot=0;
            lexicon224x::fetch(*clone);
            if(clone->fpc.count==39) loads[1]=row;
            if(clone->fpc.count==89) loads[0]=row;
            if(!clone->fpc.busy && clone->fpc.new_data) converting=waiting;
            lexicon224x::converter_clock(*clone);
            if(clone->mi.wr_da) waiting=row;
            if(clone->mi.op==lexicon224x::OPER && clone->mi.source==lexicon224x::FromADC) {
                require(adc_slot<2,"more than two XL ADC reads per pass");
                const unsigned c=adc_slot++;
                if(row>=info.rows*warm_passes) {
                    require(clone->fpc.input_sample==(c?3200:1600),"XL ADC channel/gain expansion differs from FPC");
                    require(loads[c]>=0,"missing XL ADC final load");
                    const int hold=int(loads[c])-36-int(row/info.rows)*int(info.rows);
                    if(reads[c]>=0) require(reads[c]==int(local) && holds[c]==hold,"nonperiodic XL ADC read/hold phase");
                    reads[c]=int(local);holds[c]=hold;++adc_count;
                }
            }
            if(clone->dac_channels && row>=info.rows*warm_passes) {
                require(converting>=0,"missing XL DAC request association");
                const int offset=int(row)-int(converting/info.rows)*int(info.rows);
                const unsigned request=unsigned(converting)%info.rows;
                require(request_masks[request]==clone->dac_channels,"XL FPC captured a different request channel mask");
                if(captures[request]>=0 && captures[request]!=offset) {
                    std::cerr<<info.name<<" request row "<<request<<" capture "<<row<<" request "<<converting
                        <<": phase "<<offset<<" vs previous "<<captures[request]<<'\n';
                    require(false,"nonperiodic XL FPC request/capture phase");
                }
                captures[request]=offset;++dac_events;
                for(unsigned c=0;c<4;++c) if((clone->dac_channels>>c)&1) ++dac_count;
            }
            lexicon224x::execute(*clone);
        }
        unsigned expected_events=0,expected_channels=0;
        for(unsigned r=0;r<info.rows;++r) if(captures[r]>=0) {
            ++expected_events;for(unsigned c=0;c<4;++c) if((request_masks[r]>>c)&1) ++expected_channels;
            output<<program<<','<<info.rows<<','<<reads[0]<<','<<reads[1]<<','<<holds[0]<<','<<holds[1]
                <<','<<r<<','<<request_masks[r]<<','<<captures[r]<<','<<adc_count<<','<<dac_events<<','<<dac_count<<'\n';
        }
        require(adc_count==18 && dac_events==expected_events*observed_passes && dac_count==expected_channels*observed_passes,"unexpected XL ADC/DAC event count");
        require(reads[0]>=0 && reads[1]>=0 && expected_events,"missing XL converter events");output.flush();
        std::cout<<info.name<<": ADC reads "<<reads[0]<<'/'<<reads[1]<<", holds "<<holds[0]<<'/'<<holds[1]<<"; DAC requests ";
        for(unsigned r=0;r<info.rows;++r) if(request_masks[r]) std::cout<<r<<':'<<request_masks[r]<<"->"<<captures[r]<<' ';
        std::cout<<"(periodic warmed FPC clone)\n"<<std::flush;
        total_adc+=adc_count;total_dac+=dac_count;
    }
    require(bool(output),"XL converter CSV write failed");
    std::cout<<"22 XL graphs: "<<total_adc<<" ADC and "<<total_dac<<" DAC observations; cold transient and CPU-displaced fetches excluded\n";
}
