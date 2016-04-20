// Copyright (C) 2016 Michael Trenholm-Boyle. See the LICENSE file for more.
#ifndef UNIQUE_HANDLE_H_INCLUDED
#define UNIQUE_HANDLE_H_INCLUDED

namespace std {
    template<typename T>
    class handle_traits {
    public:
        virtual T default_value() const {
            unsigned char buffer[32] = { 0 };
            return reinterpret_cast<T *>(buffer)[0];
        }
        virtual bool is_valid_value(T h) const {
            return (h != default_value());
        }
    };
    template<> class handle_traits<HANDLE> {
    public:
        HANDLE default_value() const {
            return 0;
        }
        bool is_valid_value(HANDLE h) const {
            return h && (h != INVALID_HANDLE_VALUE);
        }
    };
    template<typename T>
    class handle_delete {
    public:
        virtual void operator()(T h) const {}
    };
    template<> class handle_delete<HANDLE> {
    public:
        void operator()(HANDLE h) const {
            CloseHandle(h);
        }
    };
    template<typename T, typename Dx = handle_delete<T>, typename Tx = handle_traits<T> >
    class unique_handle {
    private:
        T handle;
        void release(T t) const {
            Tx tx;
            if (tx.is_valid_value(t)) {
                Dx dx;
                dx(t);
            }
        }
        unique_handle(unique_handle & u) {}
    public:
        unique_handle() {
            Tx tx;
            handle = tx.default_value();
        }
        unique_handle(T h) : handle(h) {}
        unique_handle(unique_handle && src) : handle(src.handle) {
            Tx tx;
            src.handle = tx.default_value();
        }
        ~unique_handle() {
            release();
        }
        T get() const {
            return handle;
        }
        unique_handle & reset(T h) {
            if (h != handle) {
                T t = handle;
                handle = h;
                release(t);
            }
            return *this;
        }
        unique_handle & release() {
            Tx tx;
            return reset(tx.default_value());
        }
        operator bool() const {
            Tx tx;
            return tx.is_valid_value(handle);
        }
        bool operator !() const {
            Tx tx;
            return !tx.is_valid_value(handle);
        }
    };
}

#endif
