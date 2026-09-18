#pragma once
#include<request.h>
#include<response.h>
#include<functional>

using middlewareFunction = std::function<void(Request&, Response&)>;