#include <application.hpp>

#include <cstdlib>

int main(int argc, char const** argv)
{
    heatx::application_t app{argc, argv};
    app.run();
    return EXIT_SUCCESS;
}
