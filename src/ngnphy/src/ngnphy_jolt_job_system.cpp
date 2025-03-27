#include <ngnphy_jolt_job_system.hpp>

#include <cppext_thread_pool.hpp>

#include <Jolt/Core/JobSystem.h>
#include <Jolt/Core/JobSystemWithBarrier.h>

#include <algorithm>

ngnphy::job_system_t::job_system_t(cppext::thread_pool_t& thread_pool,
    unsigned int const max_barriers)
    : JPH::JobSystemWithBarrier(max_barriers)
    , pool_{&thread_pool}
{
}

int ngnphy::job_system_t::GetMaxConcurrency() const
{
    return static_cast<int>(pool_->thread_count());
}

JPH::JobSystem::JobHandle ngnphy::job_system_t::CreateJob(
    [[maybe_unused]] char const* const inName,
    [[maybe_unused]] JPH::ColorArg const inColor,
    JPH::JobSystem::JobFunction const& inJobFunction,
    JPH::uint32 const inNumDependencies)
{
    JPH::JobSystem::JobHandle handle{new JPH::JobSystem::Job(inName,
        inColor,
        this,
        inJobFunction,
        inNumDependencies)};

    if (inNumDependencies == 0)
    {
        QueueJob(handle.GetPtr());
    }

    return handle;
}

void ngnphy::job_system_t::QueueJob(JPH::JobSystem::Job* const inJob)
{
    inJob->AddRef();

    pool_->submit(
        [inJob]()
        {
            inJob->Execute();
            inJob->Release();
        });
}

void ngnphy::job_system_t::QueueJobs(JPH::JobSystem::Job** inJobs,
    JPH::uint const inNumJobs)
{
    std::ranges::for_each_n(inJobs,
        inNumJobs,
        [this](JPH::JobSystem::Job* const j) { QueueJob(j); });
}

void ngnphy::job_system_t::FreeJob(JPH::JobSystem::Job* const inJob)
{
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    delete inJob;
}
