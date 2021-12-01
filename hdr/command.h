#pragma once

class Task
{
public:
    bool Solve()
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

public:
    virtual ~Task() { }
};
