#include <application.hpp>
#include <config.hpp>

#include <cstdlib>

int main()
{
    galileo::application_t app{galileo::enable_validation_layers};
    app.run();
    return EXIT_SUCCESS;
}
