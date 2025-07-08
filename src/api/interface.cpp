/*
#include "interface.h"
#include <memory>
#include <stdexcept>

// 静态成员定义
std::shared_ptr<const GlobalSet> GlobalData::data;
std::once_flag GlobalData::init;

void GlobalData::Init(GetterFunc getter) {
  if (!getter) {
    throw std::invalid_argument("GlobalData: Getter function not set");
  }
  std::call_once(init,
                 [getter] { data = std::make_unique<GlobalSet>(getter()); });
}

const GlobalSet::OutFunc GlobalData::getOutFunc() {
  if (!data) {
    throw std::out_of_range("GlobalData: Not init");
  }
  return data->outFunc;
}

const std::shared_ptr<const GlobalSet> GlobalData::getData() {
  if (!data) {
    throw std::out_of_range("GlobalData: Not init");
  }
  return data;
}
  */