#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std::string_literals;

namespace ast {

    using runtime::Closure;
    using runtime::Context;
    using runtime::ObjectHolder;

    namespace {
        const std::string ADD_METHOD = "__add__"s;
        const std::string INIT_METHOD = "__init__"s;
    }  // namespace

    //----------------------class VariableValue----------------------//
    // Вычисляет значение переменной либо цепочки вызовов полей
    // объектов id1.id2.id3.
    // Например, выражение circle.center.x - цепочка вызовов полей
    // объектов в инструкции :
    // x = circle.center.x
    //---------------------------------------------------------------//
    VariableValue::VariableValue(std::vector<std::string> dotted_ids)
        : var_names_(std::move(dotted_ids))
    {}

    VariableValue::VariableValue(const std::string& var_name)
    {
        var_names_.push_back(var_name);
    }

    ObjectHolder VariableValue::Execute(Closure& closure,
                                        Context& /*context*/) {

        if (closure.count(var_names_[0]) > 0) {
            ObjectHolder obj = closure.at(var_names_[0]);
            auto size = var_names_.size();
            if (size == 1) {
                return obj;
            }

            for (size_t i = 1; i < size; ++i) {
                runtime::ClassInstance* obj_ptr = 
                    obj.TryAs<runtime::ClassInstance>();

                if (obj_ptr != nullptr) {
                    obj = obj_ptr->Fields()[var_names_[i]];
                    continue;
                }

                throw std::runtime_error("No fields for object!"s);
            }

            return obj;
        }
        else {
            throw std::runtime_error("No variable!"s);
        }

        return {};
    }

    //------------------------class Assignment-----------------------//
    // Присваивает переменной, имя которой задано в параметре var,
    // значение выражения rv
    //---------------------------------------------------------------//
    Assignment::Assignment(std::string var,
                           std::unique_ptr<Statement> rv)
        : var_(std::move(var))
        , rv_(std::move(rv)) 
    {}

    ObjectHolder Assignment::Execute(Closure& closure,
                                     Context& context) {
        closure[var_] = rv_.get()->Execute(closure, context);
        return closure[var_];
    }

    //---------------------class FieldAssignment---------------------//
    // Присваивает полю object.field_name значение выражения rv
    //---------------------------------------------------------------//
    FieldAssignment::FieldAssignment(VariableValue object,
                                     std::string field_name,
                                     std::unique_ptr<Statement> rv)
        : object_(std::move(object))
        , field_name_(std::move(field_name))
        , rv_(std::move(rv))
    {}

    ObjectHolder FieldAssignment::Execute(Closure& closure,
                                          Context& context) {
        object_.Execute(closure, context)
               .TryAs<runtime::ClassInstance>()->Fields()[field_name_] =
            rv_->Execute(closure, context);
        return object_.Execute(closure, context)
                      .TryAs<runtime::ClassInstance>()->Fields()[field_name_];
    }

    //--------------------------class Print--------------------------//
    // Значение print
    //---------------------------------------------------------------//
    Print::Print(std::vector<std::unique_ptr<Statement>> args)
        : args_(std::move(args))
    {}

    Print::Print(std::unique_ptr<Statement> argument)
    {
        args_.push_back(std::move(argument));
    }

    std::unique_ptr<Print> Print::Variable(const std::string& name)
    {
        return std::make_unique<Print>(
            std::make_unique<VariableValue>(name));
    }

    ObjectHolder Print::Execute(Closure& closure, Context& context) {
        for (size_t i = 0; i < args_.size(); ++i) {
            auto ptr = 
                args_[i].get()->Execute(closure, context).Get();
            if (ptr != nullptr) {
                ptr->Print(context.GetOutputStream(), context);
            }
            else {
                context.GetOutputStream() << "None"s;
            }
            if (i != args_.size() - 1) {
                context.GetOutputStream() << " "s;
            }
        }
        context.GetOutputStream() << "\n"s;
        return {};
    }

    //------------------------class MethodCall-----------------------//
    // Вызывает метод object.method со списком параметров args
    //---------------------------------------------------------------//

    MethodCall::MethodCall(std::unique_ptr<Statement> object,
                           std::string method,
                           std::vector<std::unique_ptr<Statement>>
                           args)
        : object_(std::move(object))
        , method_(std::move(method))
        , args_(std::move(args)) 
    {}

    ObjectHolder MethodCall::Execute(Closure& closure,
                                     Context& context) {
        runtime::ClassInstance* obj_ptr = 
            object_.get()->Execute(closure, context)
                   .TryAs<runtime::ClassInstance>();

        if (obj_ptr && obj_ptr->HasMethod(method_, args_.size())) {
            std::vector<ObjectHolder> args;
            for (size_t i = 0; i < args_.size(); ++i) {
                args.push_back(
                    args_[i].get()->Execute(closure, context));
            }
            return obj_ptr->Call(method_, args, context);
        }

        return {};
    }

    //-----------------------class NewInstance-----------------------//
    // Создаёт новый экземпляр класса class_, передавая его
    // конструктору набор параметров args.
    // Если в классе отсутствует метод __init__ с заданным количеством
    // аргументов, то экземпляр класса создаётся без вызова
    // конструктора (поля объекта не будут проинициализированы):
    /*
    class Person:
      def set_name(name):
        self.name = name
    p = Person()
    # Поле name будет иметь значение только после вызова метода
    # set_name
    p.set_name("Ivan")
    */
    //---------------------------------------------------------------//
    NewInstance::NewInstance(const runtime::Class& class_,
                             std::vector<std::unique_ptr<Statement>>
                             args)
        : class_ref_(class_)
        , args_(std::move(args))
    {}

    NewInstance::NewInstance(const runtime::Class& class_)
        : class_ref_(class_)
    {}

    ObjectHolder NewInstance::Execute(Closure& closure,
                                      Context& context) {

        auto obj = runtime::ObjectHolder::Own(
            runtime::ClassInstance{ class_ref_ });
        runtime::ClassInstance* ptr = 
            obj.TryAs<runtime::ClassInstance>();

        if (ptr->HasMethod(INIT_METHOD, args_.size())) {
            std::vector<ObjectHolder> args;
            for (size_t i = 0; i < args_.size(); ++i) {
                args.push_back(args_[i].get()->Execute(closure,
                                                          context));
            }
            ptr->Call(INIT_METHOD, args, context);
        }

        return obj;
    }

    //------------------------class Stringify------------------------//
    // Операция str, возвращающая строковое значение своего аргумента
    //---------------------------------------------------------------//
    ObjectHolder Stringify::Execute(Closure& closure,
                                    Context& context) {
        ObjectHolder obj = 
            argument_.get()->Execute(closure, context);
        if (bool(obj)) {
            std::ostringstream string_stream;
            obj.Get()->Print(string_stream, context);
            return ObjectHolder::Own(
                runtime::String(string_stream.str()));
        }

        return ObjectHolder::Own(runtime::String("None"s));
    }

    //---------------------------class Add---------------------------//
    // Возвращает результат операции + над аргументами lhs и rhs
    //---------------------------------------------------------------//
    ObjectHolder Add::Execute(Closure& closure, Context& context) {

        auto lhs = lhs_.get()->Execute(closure, context);
        auto rhs = rhs_.get()->Execute(closure, context);

        std::optional<runtime::Number> num_res = 
            ast::GetFunctionResult<runtime::Number, runtime::Number>(
                lhs, rhs, std::plus());
        if (num_res.has_value()) {
            return ObjectHolder::Own(runtime::Number(num_res.value()));
        }

        std::optional<runtime::String> str_res = 
            ast::GetFunctionResult<runtime::String, runtime::String>(
                lhs, rhs, std::plus());
        if (str_res.has_value()) {
            return ObjectHolder::Own(runtime::String(str_res.value()));
        }

        runtime::ClassInstance* ptr = 
            lhs.TryAs<runtime::ClassInstance>();
        if (ptr != nullptr && ptr->HasMethod(ADD_METHOD, 1)) {
            return ptr->Call(ADD_METHOD, { rhs }, context);
        }

        throw std::runtime_error("Arguments are not executable!"s);
    }

    //---------------------------class Sub---------------------------//
    // Возвращает результат вычитания аргументов lhs и rhs
    //---------------------------------------------------------------//
    ObjectHolder Sub::Execute(Closure& closure, Context& context) {

        auto lhs = lhs_.get()->Execute(closure, context);
        auto rhs = rhs_.get()->Execute(closure, context);

        std::optional<runtime::Number> num_res =
            ast::GetFunctionResult<runtime::Number, runtime::Number>(
                lhs, rhs, std::minus());
        if (num_res.has_value()) {
            return ObjectHolder::Own(runtime::Number(num_res.value()));
        }

        throw std::runtime_error("Arguments are not executable!"s);
    }

    //--------------------------class Mult---------------------------//
    // Возвращает результат умножения аргументов lhs и rhs
    //---------------------------------------------------------------//
    ObjectHolder Mult::Execute(Closure& closure, Context& context) {

        auto lhs = lhs_.get()->Execute(closure, context);
        auto rhs = rhs_.get()->Execute(closure, context);

        std::optional<runtime::Number> num_res =
            ast::GetFunctionResult<runtime::Number, runtime::Number>(
                lhs, rhs, std::multiplies());
        if (num_res.has_value()) {
            return ObjectHolder::Own(runtime::Number(num_res.value()));
        }

        throw std::runtime_error("Arguments are not executable!"s);
    }

    //---------------------------class Div---------------------------//
    // Возвращает результат деления аргументов lhs и rhs
    //---------------------------------------------------------------//
    ObjectHolder Div::Execute(Closure& closure, Context& context) {

        auto lhs = lhs_.get()->Execute(closure, context);
        auto rhs = rhs_.get()->Execute(closure, context);

        auto ptr = rhs.TryAs<runtime::Number>();
        if (ptr == nullptr) {
            throw std::runtime_error("Divisor is not defined!"s);
        }
        if (ptr->GetValue() == 0) {
            throw std::runtime_error("Division by zero!"s);
        }

        std::optional<runtime::Number> num_res =
            ast::GetFunctionResult<runtime::Number, runtime::Number>(
                lhs, rhs, std::divides());
        if (num_res.has_value()) {
            return ObjectHolder::Own(runtime::Number(num_res.value()));
        }

        throw std::runtime_error("Argument (dividend) is not executable!"s);
    }

    //---------------------------class Or----------------------------//
    // Возвращает результат вычисления логической операции or над
    // lhs и rhs
    //---------------------------------------------------------------//
    ObjectHolder Or::Execute(Closure& closure, Context& context) {
        auto lhs = lhs_.get()->Execute(closure, context);
        if (runtime::IsTrue(lhs)) {
            return ObjectHolder::Own(runtime::Bool(true));
        }
        else {
            auto rhs = rhs_.get()->Execute(closure, context);
            return ObjectHolder::Own(runtime::Bool(runtime::IsTrue(rhs)));
        }
    }

    //---------------------------class And---------------------------//
    // Возвращает результат вычисления логической операции and над
    // lhs и rhs
    //---------------------------------------------------------------//
    ObjectHolder And::Execute(Closure& closure, Context& context) {
        auto lhs = lhs_.get()->Execute(closure, context);
        if (!runtime::IsTrue(lhs)) {
            return ObjectHolder::Own(runtime::Bool(false));
        }
        else {
            auto rhs = rhs_.get()->Execute(closure, context);
            return ObjectHolder::Own(runtime::Bool(runtime::IsTrue(rhs)));
        }
    }

    //---------------------------class Not---------------------------//
    // Возвращает результат вычисления логической операции not над
    // единственным аргументом операции
    //---------------------------------------------------------------//

    ObjectHolder Not::Execute(Closure& closure, Context& context) {
        auto obj = argument_.get()->Execute(closure, context);
        return ObjectHolder::Own(runtime::Bool(!runtime::IsTrue(obj)));
    }

    //-------------------------class Compound------------------------//
    // Составная инструкция (например: тело метода, содержимое ветки
    // if, либо else)
    //---------------------------------------------------------------//
    ObjectHolder Compound::Execute(Closure& closure,
                                   Context& context) {
        for (auto& stmt : statements_) {
            stmt.get()->Execute(closure, context);
        }
        return {};
    }

    //------------------------class MethodBody-----------------------//
    // Тело метода. Как правило, содержит составную инструкцию
    //---------------------------------------------------------------//

    MethodBody::MethodBody(std::unique_ptr<Statement>&& body)
        : body_(std::move(body))
    {}

    ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
        try {
            body_.get()->Execute(closure, context);
        }
        catch (ObjectHolder& e) {
            return e;
        }
        return {};
    }

    //--------------------------class Return-------------------------//
    // Выполняет инструкцию return с выражением statement
    //---------------------------------------------------------------//
    ObjectHolder Return::Execute(Closure& closure, Context& context) {
        throw statement_.get()->Execute(closure, context);
        return {};
    }

    //---------------------class ClassDefinition---------------------//
    // Объявляет класс
    //---------------------------------------------------------------//
    ClassDefinition::ClassDefinition(ObjectHolder cls) 
        : cls_(std::move(cls)) 
    {}

    ObjectHolder ClassDefinition::Execute(Closure& closure,
                                          Context& /*context*/) {
        closure[cls_.TryAs<runtime::Class>()->GetName()] = cls_;
        return {};
    }

    //--------------------------class IfElse-------------------------//
    // Инструкция if <condition> <if_body> else <else_body>
    //---------------------------------------------------------------//

    IfElse::IfElse(std::unique_ptr<Statement> condition,
                   std::unique_ptr<Statement> if_body,
                   std::unique_ptr<Statement> else_body)
        : condition_(std::move(condition))
        , if_body_(std::move(if_body))
        , else_body_(std::move(else_body))
    {}

    ObjectHolder IfElse::Execute(Closure& closure,
                                 Context& context) {
        if (runtime::IsTrue(condition_.get()->Execute(closure,
                                                      context))) {
            return if_body_.get()->Execute(closure, context);
        }
        else {
            auto ptr = else_body_.get();
            if (ptr != nullptr)
                return ptr->Execute(closure, context);
            else {
                return {};
            }
        }
    }

    //------------------------class Comparison-----------------------//
    // Операция сравнения
    //---------------------------------------------------------------//
    Comparison::Comparison(Comparator cmp,
                           std::unique_ptr<Statement> lhs,
                           std::unique_ptr<Statement> rhs)
        : BinaryOperation(std::move(lhs), std::move(rhs))
        , cmp_(cmp)
    {}

    ObjectHolder Comparison::Execute(Closure& closure,
                                     Context& context) {
        return ObjectHolder::Own(
            runtime::Bool(cmp_(lhs_.get()->Execute(closure, context),
                          rhs_.get()->Execute(closure, context), context)));
    }

}  // namespace ast