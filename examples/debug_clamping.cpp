#include "../src/format_converter.h"
#include <iostream>

using namespace wasapi::internal;

int main() {
    float input[] = {-1.5f, -1.0f, 0.0f, 1.0f, 1.5f};
    int16_t output[5];
    
    AudioFormatConverter::float32ToInt16(input, output, 5);
    
    for (int i = 0; i < 5; ++i) {
        std::cout << "input[" << i << "] = " << input[i] 
                  << " -> output[" << i << "] = " << output[i] << std::endl;
    }
    
    std::cout << "\nExpected:" << std::endl;
    std::cout << "-1.5 -> -32768 (clamped)" << std::endl;
    std::cout << "-1.0 -> -32768" << std::endl;
    std::cout << " 0.0 -> 0" << std::endl;
    std::cout << " 1.0 -> 32767" << std::endl;
    std::cout << " 1.5 -> 32767 (clamped)" << std::endl;
    
    return 0;
}
