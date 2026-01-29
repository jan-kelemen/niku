#include <application.hpp>

#include <cstdlib>

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    reshed::application_t app;
    app.run();
    return EXIT_SUCCESS;
}
