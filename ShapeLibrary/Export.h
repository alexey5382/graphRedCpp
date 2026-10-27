#pragma once

#ifdef SHAPELIB_EXPORTS
#define SHAPE_API __declspec(dllexport)
#else
#define SHAPE_API __declspec(dllimport)
#endif