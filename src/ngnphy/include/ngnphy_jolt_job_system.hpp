#ifndef NGNPHY_JOLT_JOB_SYSTEM_INCLUDED
#define NGNPHY_JOLT_JOB_SYSTEM_INCLUDED

#include <ngnphy_jolt.hpp> // IWYU pragma: keep

#include <Jolt/Core/Color.h>
#include <Jolt/Core/Core.h>
#include <Jolt/Core/JobSystem.h>
#include <Jolt/Core/JobSystemWithBarrier.h>

namespace cppext
{
    class thread_pool_t;
} // namespace cppext

namespace ngnphy
{
    class [[nodiscard]] job_system_t final : public JPH::JobSystemWithBarrier
    {
    public:
        explicit job_system_t(cppext::thread_pool_t& thread_pool,
            unsigned int max_barriers = 1);

        job_system_t(job_system_t const&) = delete;

        job_system_t(job_system_t&&) noexcept = delete;

    public:
        ~job_system_t() override = default;

    public:
        job_system_t& operator=(job_system_t const&) = delete;

        job_system_t& operator=(job_system_t&&) noexcept = delete;

    private:
        [[nodiscard]] int GetMaxConcurrency() const override;

        JPH::JobSystem::JobHandle CreateJob(char const* inName,
            JPH::ColorArg inColor,
            JPH::JobSystem::JobFunction const& inJobFunction,
            JPH::uint32 inNumDependencies) override;

        void QueueJob(JPH::JobSystem::Job* inJob) override;

        void QueueJobs(JPH::JobSystem::Job** inJobs,
            JPH::uint inNumJobs) override;

        void FreeJob(JPH::JobSystem::Job* inJob) override;

    private:
        cppext::thread_pool_t* pool_;
    };
} // namespace ngnphy

#endif
