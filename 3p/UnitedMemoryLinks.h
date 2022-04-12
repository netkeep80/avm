#include <cstdint>
#include <string>

namespace Platform::Data::Doublets::Memory::United::Generic
{

    template <typename T>
    struct Link
    {
        T index;
        T source;
        T target;
    };

    template <typename T>
    using CUDCallback = T (*)(Link<T> before, Link<T> after);

    template <typename T>
    using EachCallback = T (*)(Link<T>);

    struct SharedLogger
    {
        //void (*formatter)(const Record*);
    };

    namespace externC
    {
        extern "C"
        {
            void *ByteUnitedMemoryLinks_New(const char *path);
            void *UInt16UnitedMemoryLinks_New(const char *path);
            void *UInt32UnitedMemoryLinks_New(const char *path);
            void *UInt64UnitedMemoryLinks_New(const char *path);

            void ByteUnitedMemoryLinks_Drop(void *this_);
            void UInt16UnitedMemoryLinks_Drop(void *this_);
            void UInt32UnitedMemoryLinks_Drop(void *this_);
            void UInt64UnitedMemoryLinks_Drop(void *this_);

            uint8_t ByteUnitedMemoryLinks_Create(void *this_,
                                                 const uint8_t *query,
                                                 uintptr_t len,
                                                 CUDCallback<uint8_t> callback);

            uint16_t UInt16UnitedMemoryLinks_Create(void *this_,
                                                    const uint16_t *query,
                                                    uintptr_t len,
                                                    CUDCallback<uint16_t> callback);

            uint32_t UInt32UnitedMemoryLinks_Create(void *this_,
                                                    const uint32_t *query,
                                                    uintptr_t len,
                                                    CUDCallback<uint32_t> callback);

            uint64_t UInt64UnitedMemoryLinks_Create(void *this_,
                                                    const uint64_t *query,
                                                    uintptr_t len,
                                                    CUDCallback<uint64_t> callback);

            uint8_t ByteUnitedMemoryLinks_Each(void *this_,
                                               const uint8_t *query,
                                               uintptr_t len,
                                               EachCallback<uint8_t> callback);

            uint16_t UInt16UnitedMemoryLinks_Each(void *this_,
                                                  const uint16_t *query,
                                                  uintptr_t len,
                                                  EachCallback<uint16_t> callback);

            uint32_t UInt32UnitedMemoryLinks_Each(void *this_,
                                                  const uint32_t *query,
                                                  uintptr_t len,
                                                  EachCallback<uint32_t> callback);

            uint64_t UInt64UnitedMemoryLinks_Each(void *this_,
                                                  const uint64_t *query,
                                                  uintptr_t len,
                                                  EachCallback<uint64_t> callback);

            uint8_t ByteUnitedMemoryLinks_Count(void *this_, const uint8_t *query, uintptr_t len);
            uint16_t UInt16UnitedMemoryLinks_Count(void *this_, const uint16_t *query, uintptr_t len);
            uint32_t UInt32UnitedMemoryLinks_Count(void *this_, const uint32_t *query, uintptr_t len);
            uint64_t UInt64UnitedMemoryLinks_Count(void *this_, const uint64_t *query, uintptr_t len);

            uint8_t ByteUnitedMemoryLinks_Update(void *this_,
                                                 const uint8_t *restrictions,
                                                 uintptr_t len_r,
                                                 const uint8_t *substitutuion,
                                                 uintptr_t len_s,
                                                 CUDCallback<uint8_t> callback);

            uint16_t UInt16UnitedMemoryLinks_Update(void *this_,
                                                    const uint16_t *restrictions,
                                                    uintptr_t len_r,
                                                    const uint16_t *substitutuion,
                                                    uintptr_t len_s,
                                                    CUDCallback<uint16_t> callback);

            uint32_t UInt32UnitedMemoryLinks_Update(void *this_,
                                                    const uint32_t *restrictions,
                                                    uintptr_t len_r,
                                                    const uint32_t *substitutuion,
                                                    uintptr_t len_s,
                                                    CUDCallback<uint32_t> callback);

            uint64_t UInt64UnitedMemoryLinks_Update(void *this_,
                                                    const uint64_t *restrictions,
                                                    uintptr_t len_r,
                                                    const uint64_t *substitutuion,
                                                    uintptr_t len_s,
                                                    CUDCallback<uint64_t> callback);

            uint8_t ByteUnitedMemoryLinks_Delete(void *this_,
                                                 const uint8_t *query,
                                                 uintptr_t len,
                                                 CUDCallback<uint8_t> callback);

            uint16_t UInt16UnitedMemoryLinks_Delete(void *this_,
                                                    const uint16_t *query,
                                                    uintptr_t len,
                                                    CUDCallback<uint16_t> callback);

            uint32_t UInt32UnitedMemoryLinks_Delete(void *this_,
                                                    const uint32_t *query,
                                                    uintptr_t len,
                                                    CUDCallback<uint32_t> callback);

            uint64_t UInt64UnitedMemoryLinks_Delete(void *this_,
                                                    const uint64_t *query,
                                                    uintptr_t len,
                                                    CUDCallback<uint64_t> callback);

            void setup_shared_logger(SharedLogger logger);

            void init_fmt_logger();

        } // extern "C"
    }

    template <typename TLink>
    class UnitedMemoryLinks;

    template <>
    class UnitedMemoryLinks<uint8_t>
    {
    private:
        void *this_{nullptr};

    public:
        UnitedMemoryLinks(const std::string_view &path)
        {
            this_ = externC::ByteUnitedMemoryLinks_New(path.data());
        }

        ~UnitedMemoryLinks()
        {
            if (this_)
                externC::ByteUnitedMemoryLinks_Drop(this_);
        }
    };


    template <>
    class UnitedMemoryLinks<uint16_t>
    {
    private:
        void *this_{nullptr};

    public:
        UnitedMemoryLinks(const std::string_view &path)
        {
            this_ = externC::UInt16UnitedMemoryLinks_New(path.data());
        }

        ~UnitedMemoryLinks()
        {
            if (this_)
                externC::UInt16UnitedMemoryLinks_Drop(this_);
        }
    };


    template <>
    class UnitedMemoryLinks<uint32_t>
    {
    private:
        void *this_{nullptr};

    public:
        UnitedMemoryLinks(const std::string_view &path)
        {
            this_ = externC::UInt32UnitedMemoryLinks_New(path.data());
        }

        ~UnitedMemoryLinks()
        {
            if (this_)
                externC::UInt32UnitedMemoryLinks_Drop(this_);
        }
    };


    template <>
    class UnitedMemoryLinks<uint64_t>
    {
    private:
        void *this_{nullptr};

    public:
        UnitedMemoryLinks(const std::string_view &path)
        {
            this_ = externC::UInt64UnitedMemoryLinks_New(path.data());
        }

        ~UnitedMemoryLinks()
        {
            if (this_)
                externC::UInt64UnitedMemoryLinks_Drop(this_);
        }
    };
}
