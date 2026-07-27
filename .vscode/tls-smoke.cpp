#include <mutex>

int main()
{
    static std::once_flag flag;
    std::call_once(flag, [] {});
}
