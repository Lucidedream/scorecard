#include "GolfCourseParse.h"

#if defined(CROSSPOINT_GOLF)

#include <cstring>

#include "CourseOrder.h"
#include "GolfRoundDecode.h"

namespace {

GolfCourseValidationResult failure(GolfCourseValidationError error, GolfCourseField field) {
  return {false, error, field, 0, 0, 0};
}

bool validName(JsonVariantConst value, size_t capacity, bool comma = false) {
  if (!value.is<const char*>()) {
    return false;
  }
  const char* name = value.as<const char*>();
  return name[0] && strlen(name) < capacity && !strpbrk(name, comma ? ",\r\n" : "\r\n");
}

GolfCourseValidationError rangeErrorForField(GolfCourseField field) {
  switch (field) {
    case GolfCourseField::Par:
      return GolfCourseValidationError::ParRange;
    case GolfCourseField::Yards:
      return GolfCourseValidationError::YardageRange;
    default:
      return GolfCourseValidationError::StrokeIndexRange;
  }
}

GolfCourseValidationResult arrayValid(JsonVariantConst value, int minimum, int maximum, GolfCourseField field,
                                      bool unique = false) {
  const auto array = value.as<JsonArrayConst>();
  if (array.isNull() || array.size() != GOLF_HOLES_PER_NINE) {
    return failure(GolfCourseValidationError::ArrayLength, field);
  }
  uint32_t seen = 0;
  uint8_t hole = 0;
  for (const auto item : array) {
    if (!item.is<int>()) {
      return failure(GolfCourseValidationError::ArrayLength, field);
    }
    const int entry = item.as<int>();
    if (entry < minimum || entry > maximum) {
      return {false, rangeErrorForField(field), field, hole, static_cast<uint16_t>(entry), 0};
    }
    if (unique) {
      if (seen & (1UL << entry)) {
        return failure(GolfCourseValidationError::StrokeIndexDuplicate, field);
      }
      seen |= 1UL << entry;
    }
    ++hole;
  }
  return {true, GolfCourseValidationError::None, GolfCourseField::None, 0, 0, 0};
}

void copyValidatedName(JsonVariantConst value, char* output) {
  const char* name = value.as<const char*>();
  const size_t length = strlen(name);
  memcpy(output, name, length + 1);
}

}  // namespace

GolfCourseValidationResult golfParseCourseJson(JsonVariantConst doc, GolfCourse& course) {
  if (!doc["v"].is<int>() || doc["v"].as<int>() != 2) {
    return failure(GolfCourseValidationError::Version, GolfCourseField::None);
  }
  if (!validName(doc["name"], sizeof(course.courseName))) {
    return failure(GolfCourseValidationError::Name, GolfCourseField::Name);
  }
  const auto nines = doc["nines"].as<JsonArrayConst>();
  if (nines.isNull() || nines.size() < 1 || nines.size() > GOLF_MAX_NINES) {
    return failure(GolfCourseValidationError::NineCount, GolfCourseField::Nines);
  }
  for (size_t nineIndex = 0; nineIndex < nines.size(); ++nineIndex) {
    const auto nine = nines[nineIndex];
    if (!validName(nine["name"], GOLF_NINE_NAME_CAPACITY, true)) {
      return failure(GolfCourseValidationError::Name, GolfCourseField::NineName);
    }
    for (size_t previousIndex = 0; previousIndex < nineIndex; ++previousIndex) {
      if (strcmp(nine["name"].as<const char*>(), nines[previousIndex]["name"].as<const char*>()) == 0) {
        return failure(GolfCourseValidationError::Duplicate, GolfCourseField::NineName);
      }
    }
    auto validation = arrayValid(nine["par"], 3, 6, GolfCourseField::Par);
    if (!validation.valid) {
      return validation;
    }
    if (!nine["si"].isUnbound()) {
      validation = arrayValid(nine["si"], 1, nines.size() == 1 ? 9 : 18, GolfCourseField::StrokeIndex, true);
      if (!validation.valid) {
        return validation;
      }
    }
  }
  const auto tees = doc["tees"].as<JsonArrayConst>();
  if (!doc["tees"].isUnbound() && tees.isNull()) {
    return failure(GolfCourseValidationError::TeeCount, GolfCourseField::Tees);
  }
  if (tees.size() > GOLF_MAX_TEES) {
    return failure(GolfCourseValidationError::TeeCount, GolfCourseField::Tees);
  }
  for (size_t teeIndex = 0; teeIndex < tees.size(); ++teeIndex) {
    const auto tee = tees[teeIndex];
    if (!validName(tee["name"], GOLF_TEE_CAPACITY, true) || !golfTeeStringValid(tee["name"].as<const char*>())) {
      return failure(GolfCourseValidationError::Name, GolfCourseField::TeeName);
    }
    for (size_t previousIndex = 0; previousIndex < teeIndex; ++previousIndex) {
      if (strcmp(tee["name"].as<const char*>(), tees[previousIndex]["name"].as<const char*>()) == 0) {
        return failure(GolfCourseValidationError::Duplicate, GolfCourseField::TeeName);
      }
    }
    if (!tee["yards"].isUnbound()) {
      const auto yards = tee["yards"].as<JsonArrayConst>();
      if (yards.isNull() || yards.size() != nines.size()) {
        return failure(GolfCourseValidationError::ArrayLength, GolfCourseField::Yards);
      }
      for (const auto nineYards : yards) {
        const auto validation = arrayValid(nineYards, 1, 999, GolfCourseField::Yards);
        if (!validation.valid) {
          return validation;
        }
      }
    }
  }

  // Complete validation before writing so a rejected document leaves the course untouched.
  course = {};
  copyValidatedName(doc["name"], course.courseName);
  course.nineCount = nines.size();
  for (uint8_t nineIndex = 0; nineIndex < course.nineCount; ++nineIndex) {
    const auto source = nines[nineIndex];
    auto& destination = course.nines[nineIndex];
    copyValidatedName(source["name"], destination.name);
    destination.hasSi = !source["si"].isUnbound();
    for (uint8_t hole = 0; hole < GOLF_HOLES_PER_NINE; ++hole) {
      destination.par[hole] = source["par"][hole].as<uint8_t>();
      if (destination.hasSi) {
        destination.si[hole] = source["si"][hole].as<uint8_t>();
      }
    }
  }
  course.teeCount = tees.size();
  for (uint8_t teeIndex = 0; teeIndex < course.teeCount; ++teeIndex) {
    const auto source = tees[teeIndex];
    auto& destination = course.tees[teeIndex];
    copyValidatedName(source["name"], destination.name);
    destination.hasYards = !source["yards"].isUnbound();
    if (destination.hasYards) {
      for (uint8_t nineIndex = 0; nineIndex < course.nineCount; ++nineIndex) {
        for (uint8_t hole = 0; hole < GOLF_HOLES_PER_NINE; ++hole) {
          destination.yards[nineIndex][hole] = source["yards"][nineIndex][hole].as<uint16_t>();
        }
      }
    }
  }
  if (!course.teeCount) {
    course.teeCount = 2;
    memcpy(course.tees[0].name, "Blue", sizeof("Blue"));
    memcpy(course.tees[1].name, "White", sizeof("White"));
  }
  for (uint8_t teeIndex = 1; teeIndex < course.teeCount; ++teeIndex) {
    const GolfCourseTee value = course.tees[teeIndex];
    uint8_t position = teeIndex;
    while (position && golfTeeSortsBefore(value, course.tees[position - 1])) {
      course.tees[position] = course.tees[position - 1];
      --position;
    }
    course.tees[position] = value;
  }
  return {true, GolfCourseValidationError::None, GolfCourseField::None, 0, 0, 0};
}

#endif
