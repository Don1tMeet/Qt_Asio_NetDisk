#ifndef DISALLOWCOPYANDMOVE_H
#define DISALLOWCOPYANDMOVE_H

// 禁止拷贝和移动
#define DISALLOW_COPY_AND_MOVE(TypeName) \
    TypeName(const TypeName&) = delete;   \
    TypeName& operator=(const TypeName&) = delete; \
    TypeName(TypeName&&) = delete;        \
    TypeName& operator=(TypeName&&) = delete

#endif // DISALLOWCOPYANDMOVE_H
