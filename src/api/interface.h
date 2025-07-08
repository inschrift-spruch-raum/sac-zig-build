/*
#pragma once
#include <functional>
#include <memory>
#include <mutex>
#include <span>

struct GlobalSet {
  using OutFunc = std::function<bool(std::span<const char>)>;

  GlobalSet(OutFunc outFunc) : outFunc(outFunc) {}

  OutFunc outFunc;

  GlobalSet() = delete;
};

class GlobalData {
public:
  using GetterFunc = std::function<GlobalSet()>;

  static void Init(GetterFunc getter);

  static const std::shared_ptr<const GlobalSet> getData();
  static const GlobalSet::OutFunc getOutFunc();

  // 禁止拷贝和赋值
  GlobalData(const GlobalData &) = delete;
  GlobalData &operator=(const GlobalData &) = delete;

private:
  static std::shared_ptr<const GlobalSet> data;
  static std::once_flag init;

  GlobalData() = delete;
  ~GlobalData() = delete;
};
*/