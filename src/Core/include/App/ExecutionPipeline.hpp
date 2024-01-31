#ifndef EXECUTION_PIPELINE_HPP
#define EXECUTION_PIPELINE_HPP

namespace ettycc
{
    class ExecutionPipeline
    {
    public:
        virtual void Init() = 0; // TODO: NOT WIRED TO SDL APP 
        virtual void UpdateUI() = 0;
        virtual void Update() = 0; // TODO: NOT WIRED TO SDL APP 
    };
} // namespace ettycc

#endif