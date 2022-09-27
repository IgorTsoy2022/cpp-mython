#include "runtime.h"

#include <algorithm>
#include <cassert>
#include <optional>
#include <sstream>

using namespace std::literals;

namespace runtime {

    //-----------------------class ObjectHolder----------------------//
    // Специальный класс-обёртка, предназначенный для хранения       //
    // объекта в Mython-программе                                    //
    //---------------------------------------------------------------//

    ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
        : data_(std::move(data))
    {}

    void ObjectHolder::AssertIsValid() const {
        assert(data_ != nullptr);
    }

    ObjectHolder ObjectHolder::Share(Object& object) {
        // Возвращаем невладеющий shared_ptr (его deleter ничего не
        // делает)
        return ObjectHolder(std::shared_ptr<Object>(&object,
            [](auto* /*p*/) { /* do nothing */ }));
    }

    ObjectHolder ObjectHolder::None() {
        return ObjectHolder();
    }

    Object& ObjectHolder::operator*() const {
        AssertIsValid();
        return *Get();
    }

    Object* ObjectHolder::operator->() const {
        AssertIsValid();
        return Get();
    }

    Object* ObjectHolder::Get() const {
        return data_.get();
    }

    ObjectHolder::operator bool() const {
        return Get() != nullptr;
    }

    //---------------------------------------------------------------//

    bool IsTrue(const ObjectHolder& object) {

        if (!bool(object) ||
            object.TryAs<Class>() ||
            object.TryAs<ClassInstance>()) {
            return false;
        }

        auto bool_ptr = object.TryAs<Bool>();
        if (bool_ptr != nullptr) {
            return bool_ptr->GetValue();
        }

        auto num_ptr = object.TryAs<Number>();
        if (num_ptr != nullptr) {
            return (num_ptr->GetValue() == 0) ? false : true;
        }

        auto str_ptr = object.TryAs<String>();
        if (str_ptr != nullptr) {
            return (str_ptr->GetValue() == ""s) ? false : true;
        }

        return true;
    }

    //---------------------------class Bool--------------------------//
    // Логическое значение                                           //
    //---------------------------------------------------------------//

    void Bool::Print(std::ostream& os,
        [[maybe_unused]] Context& context) {
        os << (GetValue() ? "True"sv : "False"sv);
    }

    //---------------------------------------------------------------//

    //--------------------------class Class--------------------------//
    // Класс                                                         //
    //---------------------------------------------------------------//

    Class::Class(std::string name, std::vector<Method> methods,
        const Class* parent)
        : name_(name)
        , methods_(std::move(methods))
        , parent_ptr_(parent)
    {}

    const Method* Class::GetMethod(const std::string& name) const {
        const auto it = std::find_if(methods_.begin(), methods_.end(),
            [&name](const Method& m) {
                return m.name == name;
            });

        if (it != methods_.end()) {
            return &(*it);
        }

        if (parent_ptr_ != nullptr) {
            const Method* ptr = parent_ptr_->GetMethod(name);
            if (ptr != nullptr) {
                return ptr;
            }
        }

        return nullptr;
    }

    void Class::Print(std::ostream& os, Context& /*context*/) {
        os << "Class "s << GetName();
    }

    //---------------------------------------------------------------//

    //----------------------class ClassInstance----------------------//
    // Экземпляр класса                                              //
    //---------------------------------------------------------------//

    ClassInstance::ClassInstance(const Class& cls)
        : cls_(cls)
    {
        closure_["self"] = ObjectHolder::Share(*this);
    }

    void ClassInstance::Print(std::ostream& os, Context& context) {
        if (HasMethod("__str__", 0)) {
            Call("__str__", {}, context).Get()->Print(os, context);
        }
        else {
            os << this;
        }
    }

    ObjectHolder ClassInstance::Call(const std::string& method,
        const std::vector<ObjectHolder>& actual_args,
        Context& context) {
        if (HasMethod(method, actual_args.size())) {
            const Method* ptr = cls_.GetMethod(method);
            Closure local_closure;
            local_closure["self"] = ObjectHolder::Share(*this);
            for (size_t i = 0; i < actual_args.size(); ++i) {
                local_closure[ptr->formal_params[i]] = actual_args[i];
            }
            return ptr->body->Execute(local_closure, context);
        }

        throw std::runtime_error("Not implemented"s);
    }

    bool ClassInstance::HasMethod(const std::string& method,
        size_t argument_count) const {
        const Method* ptr = cls_.GetMethod(method);
        return (ptr != nullptr && ptr->formal_params.size() ==
            argument_count) ? true : false;
    }

    Closure& ClassInstance::Fields() {
        return closure_;
    }

    const Closure& ClassInstance::Fields() const {
        return closure_;
    }

    //---------------------------------------------------------------//

    template<typename F>
    std::optional<bool> CompareObjects(const ObjectHolder& lhs,
        const ObjectHolder& rhs,
        F comparator) {
        Bool* lhs_bool_ptr = lhs.TryAs<Bool>();
        Bool* rhs_bool_ptr = rhs.TryAs<Bool>();
        if (lhs_bool_ptr && rhs_bool_ptr) {
            return comparator(lhs_bool_ptr->GetValue(),
                rhs_bool_ptr->GetValue());
        }

        Number* lhs_num_ptr = lhs.TryAs<Number>();
        Number* rhs_num_ptr = rhs.TryAs<Number>();
        if (lhs_num_ptr && rhs_num_ptr) {
            return comparator(lhs_num_ptr->GetValue(),
                rhs_num_ptr->GetValue());
        }

        String* lhs_str_ptr = lhs.TryAs<String>();
        String* rhs_str_ptr = rhs.TryAs<String>();
        if (lhs_str_ptr && rhs_str_ptr) {
            return comparator(lhs_str_ptr->GetValue(),
                rhs_str_ptr->GetValue());
        }

        return std::nullopt;
    }

    bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs,
        Context& context) {
        ClassInstance* class_instance_ptr = lhs.TryAs<ClassInstance>();
        if (class_instance_ptr) {
            return IsTrue(class_instance_ptr->Call("__eq__", { rhs },
                context));
        }

        auto result = CompareObjects(lhs, rhs, std::equal_to{});
        if (result) {
            return result.value();
        }

        if (!bool(lhs) && !bool(rhs)) {
            return true;
        }

        throw std::runtime_error("Cannot compare objects for equality"s);
    }

    bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs,
        Context& context) {
        ClassInstance* class_instance_ptr = lhs.TryAs<ClassInstance>();
        if (class_instance_ptr) {
            return IsTrue(class_instance_ptr->Call("__lt__", { rhs },
                context));
        }

        auto result = CompareObjects(lhs, rhs, std::less{});
        if (result) {
            return result.value();
        }

        throw std::runtime_error("Cannot compare objects for less"s);
    }

    bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs,
        Context& context) {
        return !Equal(lhs, rhs, context);
    }

    bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs,
        Context& context) {
        return !LessOrEqual(lhs, rhs, context);
    }

    bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs,
        Context& context) {
        return Less(lhs, rhs, context) || Equal(lhs, rhs, context);
    }

    bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs,
        Context& context) {
        return !Less(lhs, rhs, context);
    }

}  // namespace runtime