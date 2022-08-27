#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>
#include <algorithm>

using namespace std;

namespace runtime {

ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
    : data_(std::move(data)) {
}

void ObjectHolder::AssertIsValid() const {
    assert(data_ != nullptr);
}

ObjectHolder ObjectHolder::Share(Object& object) {
    // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
    return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
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

bool IsTrue(const ObjectHolder& object) {
    if(auto obj = object.TryAs<Bool>(); obj != nullptr)
        return obj->GetValue();
    if(auto obj = object.TryAs<Number>(); obj != nullptr)
        return obj->GetValue() != 0;
    if(auto obj = object.TryAs<String>(); obj != nullptr)
        return obj->GetValue() != ""s;
    return false;
}

void ClassInstance::Print(std::ostream& os, Context& context) {
    if(HasMethod("__str__"s, 0)) {
        auto result = Call("__str__"s, {}, context);
        if(result.Get() != nullptr)
            result.Get()->Print(os, context);
        else
            os << "None"s;
    } else {
        os << this;
    }
}

bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
    auto result = cls_.GetMethod(method);
    if(result != nullptr && result->formal_params.size() == argument_count)
        return true;
    return false;
}

Closure& ClassInstance::Fields() {
    return closure_;
}

const Closure& ClassInstance::Fields() const {
    return closure_;
}

ClassInstance::ClassInstance(const Class& cls) : cls_(cls) {}

ObjectHolder ClassInstance::Call(const std::string& method,
                                 const std::vector<ObjectHolder>& actual_args,
                                 Context& context) {
    if(!HasMethod(method, actual_args.size()))
        throw std::runtime_error("Not implemented"s);

    auto method_ = cls_.GetMethod(method);

    Closure closure{{"self"s, ObjectHolder::Share(*this)}};

    for(size_t i = 0; i < actual_args.size(); ++i)
        closure.insert({method_->formal_params[i], actual_args[i]});

    return method_->body->Execute(closure, context);
}

Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
    : name_(name), methods_(std::move(methods)), parent_(parent) {}

const Method* Class::GetMethod(const std::string& name) const {
    for(auto& method : methods_)
        if(name == method.name) return &method;

    if(parent_ != nullptr)
        return parent_->GetMethod(name);

    return nullptr;
}

[[nodiscard]] const std::string& Class::GetName() const {
    return name_;
}

void Class::Print(ostream& os, [[maybe_unused]] Context& context) {
    os << "Class "s << name_;
}

void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
    os << (GetValue() ? "True"sv : "False"sv);
}

bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {

    if(!lhs.operator bool()) {
        if(!rhs.operator bool()) return true;
        else throw std::runtime_error("Cannot compare objects for equality"s);
    }

    {
        auto lhs_ = lhs.TryAs<Bool>();
        auto rhs_ = rhs.TryAs<Bool>();
        if(lhs_ != nullptr) {
            if(rhs_ != nullptr)
                return lhs_->GetValue() == rhs_->GetValue();
            else
                throw std::runtime_error("Cannot compare objects for equality"s);
        }
    }
    {
        auto lhs_ = lhs.TryAs<Number>();
        auto rhs_ = rhs.TryAs<Number>();
        if(lhs_ != nullptr) {
            if(rhs_ != nullptr)
                return lhs_->GetValue() == rhs_->GetValue();
            else
                throw std::runtime_error("Cannot compare objects for equality"s);
        }
    }
    {
        auto lhs_ = lhs.TryAs<String>();
        auto rhs_ = rhs.TryAs<String>();
        if(lhs_ != nullptr) {
            if(rhs_ != nullptr)
                return lhs_->GetValue() == rhs_->GetValue();
            else
                throw std::runtime_error("Cannot compare objects for equality"s);
        }
    }
    {
        auto lhs_ = lhs.TryAs<ClassInstance>();
        if(lhs_ != nullptr) {
            if(lhs_->HasMethod("__eq__"s, 1)) {
                auto result = lhs_->Call("__eq__"s, {rhs}, context);
                return result.TryAs<Bool>()->GetValue();
            } else
                throw std::runtime_error("Cannot compare objects for equality"s);
        }
    }

    throw std::runtime_error("Cannot compare objects for equality"s);
}

bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {

    if(!lhs.operator bool())
        throw std::runtime_error("Cannot compare objects for less"s);

    {
        auto lhs_ = lhs.TryAs<Bool>();
        auto rhs_ = rhs.TryAs<Bool>();
        if(lhs_ != nullptr) {
            if(rhs_ != nullptr)
                return lhs_->GetValue() < rhs_->GetValue();
            else
                throw std::runtime_error("Cannot compare objects for less"s);
        }
    }
    {
        auto lhs_ = lhs.TryAs<Number>();
        auto rhs_ = rhs.TryAs<Number>();
        if(lhs_ != nullptr) {
            if(rhs_ != nullptr)
                return lhs_->GetValue() < rhs_->GetValue();
            else
                throw std::runtime_error("Cannot compare objects for less"s);
        }
    }
    {
        auto lhs_ = lhs.TryAs<String>();
        auto rhs_ = rhs.TryAs<String>();
        if(lhs_ != nullptr) {
            if(rhs_ != nullptr)
                return lhs_->GetValue() < rhs_->GetValue();
            else
                throw std::runtime_error("Cannot compare objects for less"s);
        }
    }
    {
        auto lhs_ = lhs.TryAs<ClassInstance>();
        if(lhs_ != nullptr) {
            if(lhs_->HasMethod("__lt__"s, 1)) {
                auto result = lhs_->Call("__lt__"s, {rhs}, context);
                return result.TryAs<Bool>()->GetValue();
            } else
                throw std::runtime_error("Cannot compare objects for equality"s);
        }
    }

    throw std::runtime_error("Cannot compare objects for less"s);
}

bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Equal(lhs, rhs, context);
}

bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context) && !Equal(lhs, rhs, context);
}

bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Greater(lhs, rhs, context);
}

bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context);
}

}  // namespace runtime
