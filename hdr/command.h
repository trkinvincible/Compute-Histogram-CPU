#pragma once

class Task
{
public:

    static const std::size_t NO_OF_CORES;

    virtual ~Task() { }

    bool Compute()
    {
        if (!ParseInput()) {
            return false;
        }

        if (!Operate()) {
            return false;
        }

        WriteOutput();

        return true;
    }

protected:
    virtual bool ParseInput() = 0;
    virtual bool Operate() = 0;
    virtual void WriteOutput() = 0;

};
