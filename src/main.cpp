#include <exception>
#include <iostream>

#include "app.hpp"

int main(int argc, char **argv) {
  app_t *app = new app_t(argc, (const char **)(argv));
  try {
    app->run();
  } catch (const std::exception &e) {
    std::cout << e.what() << '\n';
  }
  delete app;
  return 0;
}
