// Run the actual coroutine and silent-import regressions without JUCE or ROMs.
#include "../import/xl_import.hpp"
#include <iostream>
#include <exception>

int main() try {
    native_hall::import::Callbacks callbacks;
    callbacks.progress=[](double,const char* stage) {std::cout<<stage<<std::endl;return true;};
    const auto stats=cineol::xl::import::check_preparation_runtime(callbacks);
    std::cout<<"XL runtime: largest frame="<<stats.largest_frame
             <<", peak frames="<<stats.peak_frames<<"; pass\n";
} catch(const std::exception& error) {
    std::cerr<<error.what()<<'\n';return 1;
}
