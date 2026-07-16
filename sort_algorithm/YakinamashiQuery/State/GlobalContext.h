// GlobalContext.h
#pragma once

class GlobalContext {
private:
    GlobalContext() = default;
public:
    static GlobalContext& instance() {
        static GlobalContext inst;
        return inst;
    }
    // コピー・代入禁止
    GlobalContext(const GlobalContext&) = delete;
    void operator=(const GlobalContext&) = delete;
};
