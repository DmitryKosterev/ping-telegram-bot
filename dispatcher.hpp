// dispatcher.hpp
#pragma once
#include <functional>
void eventDispatcher();
void pushEvent(std::function<void()> func);
