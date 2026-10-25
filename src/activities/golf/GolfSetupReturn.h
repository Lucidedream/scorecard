#pragma once

#include "GolfNavigation.h"
#include "golf/CourseStore.h"

struct GolfSetupReturn {
  GolfPickerContext picker{};
  GolfCourseFile infoFile{};
  uint16_t infoRow = 0;
  bool toInfo = false;

  void selectPicker(GolfPickerContext context) {
    picker = context;
    toInfo = false;
  }
  void selectInfo(const GolfCourseFile& file, uint16_t row) {
    infoFile = file;
    infoRow = row;
    toInfo = true;
  }
  void parent() {
    picker.level = GolfPickerLevel::Hub;
    picker.selected = picker.parentSelected;
    toInfo = false;
  }
};
static_assert(sizeof(GolfSetupReturn) < 128);
