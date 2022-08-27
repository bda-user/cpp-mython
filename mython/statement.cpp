#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

using runtime::Closure;
using runtime::Context;
using runtime::ObjectHolder;

namespace {
const string ADD_METHOD = "__add__"s;
const string INIT_METHOD = "__init__"s;
}  // namespace

ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
    return closure[name_] = value_->Execute(closure, context);
}

Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv)
    : name_(std::move(var)), value_(std::move(rv)) {}

VariableValue::VariableValue(const std::string& var_name)
    : var_name_(std::move(var_name)) {}

VariableValue::VariableValue(std::vector<std::string> dotted_ids)
    : dotted_ids_(std::move(dotted_ids)) {}

ObjectHolder VariableValue::Execute(Closure& closure, [[maybe_unused]] Context& context) {
    if(var_name_ != ""s) {
        if(auto it = closure.find(var_name_); it != closure.end())
            return it->second;
    } else
    if(dotted_ids_.size() > 0) {
        ObjectHolder result;
        Closure* closure_ = &closure;
        for(const auto& id : dotted_ids_) {
            if(auto it = closure_->find(id); it != closure_->end()) {
                result = it->second;
                auto obj = result.TryAs<runtime::ClassInstance>();
                if(obj != nullptr)
                    closure_ = &obj->Fields();
            } else
                throw runtime_error("VariableValue(dotted.?)"s);
        }
        return result;
    }

    throw runtime_error("VariableValue(?)"s);
}

unique_ptr<Print> Print::Variable(const std::string& name) {
    return make_unique<Print>(make_unique<VariableValue>(name));
}

Print::Print(unique_ptr<Statement> argument) {
    args_.push_back(std::move(argument));
}

Print::Print(vector<unique_ptr<Statement>> args) : args_(std::move(args)) {}

ObjectHolder Print::Execute(Closure& closure, Context& context) {
    bool first = true;
    for(const auto& arg : args_) {
        if(first) first = false;
        else context.GetOutputStream() << " "s;

        auto obj_hold = arg->Execute(closure, context);
        auto obj = obj_hold.Get();
        if(obj == nullptr) {
            context.GetOutputStream() << "None"s;
        } else {
            obj->Print(context.GetOutputStream(), context);
        }
    }
    context.GetOutputStream() << "\n"s;
    return {};
}

MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
                       std::vector<std::unique_ptr<Statement>> args)
    : object_(std::move(object)), method_(std::move(method)), args_(std::move(args)) {}

ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
    auto obj = object_->Execute(closure, context).TryAs<runtime::ClassInstance>();
    if(obj == nullptr)
        throw runtime_error("MethodCall(? object,,)"s);

    if(!obj->HasMethod(method_, args_.size()))
        throw runtime_error("MethodCall(,? method,)"s);

    std::vector<ObjectHolder> args;
    for (auto& arg : args_)
        args.push_back(arg.get()->Execute(closure, context));

    return obj->Call(method_, args, context);
}

ObjectHolder Stringify::Execute(Closure& closure, Context& context) {

    auto obj_hold = argument_->Execute(closure, context);
    auto obj = obj_hold.Get();

    if(obj != nullptr) {
        std::stringstream s;
        obj->Print(s, context);
        return ObjectHolder::Own(runtime::String{s.str()});
    } else {
        return ObjectHolder::Own(runtime::String{"None"s});
    }

    return {};
}

ObjectHolder Add::Execute(Closure& closure, Context& context) {
    auto lhs_obj_hold = lhs_->Execute(closure, context);
    auto rhs_obj_hold = rhs_->Execute(closure, context);
    {
        auto lhs_obj = lhs_obj_hold.TryAs<runtime::Number>();
        if(lhs_obj != nullptr) {
            auto rhs_obj = rhs_obj_hold.TryAs<runtime::Number>();
            if(rhs_obj != nullptr) {
                return ObjectHolder::Own(runtime::Number{lhs_obj->GetValue() +
                                                         rhs_obj->GetValue()});
            } else {
                throw runtime_error("Add(Number, ?)"s);
            }
        }
    }
    {
        auto lhs_obj = lhs_obj_hold.TryAs<runtime::String>();
        if(lhs_obj != nullptr) {
            auto rhs_obj = rhs_obj_hold.TryAs<runtime::String>();
            if(rhs_obj != nullptr) {
                return ObjectHolder::Own(runtime::String{lhs_obj->GetValue() +
                                                         rhs_obj->GetValue()});
            } else {
                throw runtime_error("Add(String, ?)"s);
            }
        }
    }
    {
        auto lhs_obj = lhs_obj_hold.TryAs<runtime::ClassInstance>();
        if(lhs_obj != nullptr) {
            if(lhs_obj->HasMethod("__add__"s, 1)) {
                return lhs_obj->Call("__add__"s, {std::move(rhs_obj_hold)}, context);
            }
        }
    }
    throw runtime_error("Add(?, ?)"s);
}

ObjectHolder Sub::Execute(Closure& closure, Context& context) {
    auto lhs_obj_hold = lhs_->Execute(closure, context);
    auto rhs_obj_hold = rhs_->Execute(closure, context);
    {
        auto lhs_obj = lhs_obj_hold.TryAs<runtime::Number>();
        if(lhs_obj != nullptr) {
            auto rhs_obj = rhs_obj_hold.TryAs<runtime::Number>();
            if(rhs_obj != nullptr) {
                return ObjectHolder::Own(runtime::Number{lhs_obj->GetValue() -
                                                         rhs_obj->GetValue()});
            } else {
                throw runtime_error("Sub(Number, ?)"s);
            }
        }
    }
    throw runtime_error("Sub(?, ?)"s);
}

ObjectHolder Mult::Execute(Closure& closure, Context& context) {
    auto lhs_obj_hold = lhs_->Execute(closure, context);
    auto rhs_obj_hold = rhs_->Execute(closure, context);
    {
        auto lhs_obj = lhs_obj_hold.TryAs<runtime::Number>();
        if(lhs_obj != nullptr) {
            auto rhs_obj = rhs_obj_hold.TryAs<runtime::Number>();
            if(rhs_obj != nullptr) {
                return ObjectHolder::Own(runtime::Number{lhs_obj->GetValue() *
                                                         rhs_obj->GetValue()});
            } else {
                throw runtime_error("Mult(Number, ?)"s);
            }
        }
    }
    throw runtime_error("Mult(?, ?)"s);
}

ObjectHolder Div::Execute(Closure& closure, Context& context) {
    auto lhs_obj_hold = lhs_->Execute(closure, context);
    auto rhs_obj_hold = rhs_->Execute(closure, context);
    {
        auto lhs_obj = lhs_obj_hold.TryAs<runtime::Number>();
        if(lhs_obj != nullptr) {
            auto rhs_obj = rhs_obj_hold.TryAs<runtime::Number>();
            if(rhs_obj != nullptr) {
                if(rhs_obj->GetValue() == 0)
                    throw runtime_error("Div(Number, 0): divide by zero"s);

                return ObjectHolder::Own(runtime::Number{lhs_obj->GetValue() /
                                                         rhs_obj->GetValue()});
            } else {
                throw runtime_error("Div(Number, ?)"s);
            }
        }
    }
    throw runtime_error("Div(?, ?)"s);
}

ObjectHolder Compound::Execute(Closure& closure, Context& context) {
    for(const auto& stmt : stmts_)
        stmt->Execute(closure, context);
    return {};
}

ObjectHolder Return::Execute(Closure& closure, Context& context) {
    closure["return"s] = statement_->Execute(closure, context);
    throw runtime_error("return"s);
}

ClassDefinition::ClassDefinition(ObjectHolder cls) : cls_(std::move(cls)) {}

ObjectHolder ClassDefinition::Execute(Closure& closure, Context& context) {
    auto obj = cls_.TryAs<runtime::Class>();
    return closure[obj->GetName()] =
            make_unique<NewInstance>(*obj)->Execute(closure, context);
}

FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
                                 std::unique_ptr<Statement> rv)
    : object_(std::move(object)), name_(std::move(field_name)), value_(std::move(rv)) {}

ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
    const auto obj = object_.Execute(closure, context).TryAs<runtime::ClassInstance>();

    if(obj == nullptr)
        throw std::runtime_error("FieldAssignment: haven't object"s);

    return obj->Fields()[name_] = value_->Execute(closure, context);
}

IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
               std::unique_ptr<Statement> else_body)
    : condition_(std::move(condition)), if_body_(std::move(if_body)),
      else_body_(std::move(else_body)) {}

ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
    auto cond = condition_->Execute(closure, context).TryAs<runtime::Bool>();
    if(cond->GetValue())
        return if_body_->Execute(closure, context);
    else if(else_body_ != nullptr)
        return else_body_->Execute(closure, context);
    else
        return {};
}

ObjectHolder Or::Execute(Closure& closure, Context& context) {
    auto lhs_obj_hold = lhs_->Execute(closure, context);
    auto lhs_obj = lhs_obj_hold.TryAs<runtime::Bool>();
    if(lhs_obj != nullptr) {
        if(lhs_obj->GetValue()) {
            return lhs_obj_hold;
        }

        auto rhs_obj_hold = rhs_->Execute(closure, context);
        auto rhs_obj = rhs_obj_hold.TryAs<runtime::Bool>();
        if(rhs_obj != nullptr) {
            return rhs_obj_hold;
        }
    }
    throw runtime_error("Or(?, ?)"s);;
}

ObjectHolder And::Execute(Closure& closure, Context& context) {
    auto lhs_obj_hold = lhs_->Execute(closure, context);
    auto lhs_obj = lhs_obj_hold.TryAs<runtime::Bool>();
    if(lhs_obj != nullptr) {
        if(!lhs_obj->GetValue()) {
            return lhs_obj_hold;
        }

        auto rhs_obj_hold = rhs_->Execute(closure, context);
        auto rhs_obj = rhs_obj_hold.TryAs<runtime::Bool>();
        if(rhs_obj != nullptr) {
            return rhs_obj_hold;
        }
    }
    throw runtime_error("And(?, ?)"s);;
}

ObjectHolder Not::Execute(Closure& closure, Context& context) {
    auto arg = argument_->Execute(closure, context).TryAs<runtime::Bool>();
    if(arg != nullptr) {
        return ObjectHolder::Own(runtime::Bool{arg->GetValue() ? false : true});
    }
    throw runtime_error("Not(?)"s);;
}

Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
    : BinaryOperation(std::move(lhs), std::move(rhs)), cmp_(std::move(cmp)) {}

ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
    auto lhs = lhs_->Execute(closure, context);
    auto rhs = rhs_->Execute(closure, context);
    return ObjectHolder::Own(runtime::Bool(cmp_(lhs, rhs, context)));
}

NewInstance::NewInstance(const runtime::Class& cls,
                         std::vector<std::unique_ptr<Statement>> args)
    : class_(cls), args_(std::move(args)) {}

NewInstance::NewInstance(const runtime::Class& cls) : class_(cls) {}

ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
    auto result = ObjectHolder().Own(runtime::ClassInstance(class_));

    auto new_obj = result.TryAs<runtime::ClassInstance>();
    if (new_obj != nullptr && new_obj->HasMethod(INIT_METHOD, args_.size())) {

        std::vector<ObjectHolder> args;
        for (auto& arg : args_)
            args.push_back(arg.get()->Execute(closure, context));

        new_obj->Call(INIT_METHOD, args, context);
    }

    return result;
}

MethodBody::MethodBody(std::unique_ptr<Statement>&& body)
    : body_(std::move(body)) {}

ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
    try {
        body_->Execute(closure, context);
        return {};
    }  catch (const std::runtime_error& e) {
        if(e.what() == "return"s) {
            if(auto it = closure.find("return"s); it != closure.end())
                return it->second;
        }
        return {};
    }
}

}  // namespace ast
