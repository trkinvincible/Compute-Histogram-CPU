#pragma once

#include <iostream>
#include <chrono>

// Template pattern for all task eg: compute histogram, quantize, convert , save etc..
class Task
{
public:
    static const std::size_t NO_OF_CORES;

    virtual ~Task(){ }

    bool Compute(){

        auto start = std::chrono::high_resolution_clock::now();
        if (!ParseInput()) {
            std::cerr << "Input data parse error!!!" << std::endl;
            return false;
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end-start);
        std::cout << "ParseInput completed in : " << diff.count() << " milliseconds." << std::endl;

        start = std::chrono::high_resolution_clock::now();
        if (!Operate()) {
            return false;
        }
        end = std::chrono::high_resolution_clock::now();
        diff = std::chrono::duration_cast<std::chrono::milliseconds>(end-start);
        std::cout << "Operate completed in : " << diff.count() << " milliseconds." << std::endl;

        start = std::chrono::high_resolution_clock::now();
        WriteOutput();
        end = std::chrono::high_resolution_clock::now();
        diff = std::chrono::duration_cast<std::chrono::milliseconds>(end-start);
        std::cout << "WriteOutput completed in : " << diff.count() << " milliseconds." << std::endl;

        return true;
    }

#ifdef RUN_CATCH
    virtual std::size_t OutputVal() = 0;
#endif

protected:
    virtual bool ParseInput() = 0;
    virtual bool Operate() = 0;
    virtual void WriteOutput() = 0;
};
