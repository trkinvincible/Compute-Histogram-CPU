#pragma once

#include <iostream>

// Template pattern for all task eg: compute histogram, quantize, convert , save etc..
class Task
{
public:
    static const std::size_t NO_OF_CORES;

    virtual ~Task(){ }

    bool Compute(){

        if (!ParseInput()) {
            std::cerr << "Input data parse error!!!" << std::endl;
            return false;
        }

        if (!Operate()) {
            return false;
        }

        WriteOutput();

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
