#pragma once
#if defined(CROSSPOINT_GOLF)
#include <ArduinoJson.h>

#include "GolfCourseValidate.h"
GolfCourseValidationResult golfParseCourseJson(JsonVariantConst doc, GolfCourse& course);
#endif
