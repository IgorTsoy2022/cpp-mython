#pragma once

#include "runtime.h"

#include <functional>
#include <optional>

namespace ast {

    template<typename T, typename U, typename F>
    std::optional<U> GetFunctionResult(const runtime::ObjectHolder& lhs,
                                       const runtime::ObjectHolder& rhs,
                                       F function_object) {
        T* lhs_ptr = lhs.TryAs<T>();
        T* rhs_ptr = rhs.TryAs<T>();
        if (rhs_ptr != nullptr && lhs_ptr != nullptr) {
            return function_object(lhs_ptr->GetValue(),
                                   rhs_ptr->GetValue());
        }
        return std::nullopt;
    }

    using Statement = runtime::Executable;

    //----------------------class ValueStatement---------------------//
    // ���������, ������������ �������� ���� T,
    // ������������ ��� ������ ��� �������� ��������
    //---------------------------------------------------------------//
    template <typename T>
    class ValueStatement : public Statement {
    public:
        explicit ValueStatement(T v)
            : value_(std::move(v))
        {}

        runtime::ObjectHolder Execute(runtime::Closure& /*closure*/,
            runtime::Context& /*context*/) override {
            return runtime::ObjectHolder::Share(value_);
        }

    private:
        T value_;
    };

    using NumericConst = ValueStatement<runtime::Number>;
    using StringConst = ValueStatement<runtime::String>;
    using BoolConst = ValueStatement<runtime::Bool>;

    //----------------------class VariableValue----------------------//
    // ��������� �������� ���������� ���� ������� ������� �����
    // �������� id1.id2.id3.
    // ��������, ��������� circle.center.x - ������� ������� �����
    // �������� � ���������� :
    // x = circle.center.x
    //---------------------------------------------------------------//
    class VariableValue : public Statement {
    public:
        explicit VariableValue(const std::string& var_name);

        explicit VariableValue(std::vector<std::string> dotted_ids);

        runtime::ObjectHolder 
        Execute(runtime::Closure& closure,
                runtime::Context& context) override;

    private:
        std::vector<std::string> var_names_;
    };

    //------------------------class Assignment-----------------------//
    // ����������� ����������, ��� ������� ������ � ��������� var,
    // �������� ��������� rv
    //---------------------------------------------------------------//
    class Assignment : public Statement {
    public:
        Assignment(std::string var, std::unique_ptr<Statement> rv);

        runtime::ObjectHolder
        Execute(runtime::Closure& closure,
                runtime::Context& context) override;

    private:
        std::string var_;
        std::unique_ptr<Statement> rv_;
    };

    //---------------------class FieldAssignment---------------------//
    // ����������� ���� object.field_name �������� ��������� rv
    //---------------------------------------------------------------//
    class FieldAssignment : public Statement {
    public:
        FieldAssignment(VariableValue object, std::string field_name,
                        std::unique_ptr<Statement> rv);

        runtime::ObjectHolder 
        Execute(runtime::Closure& closure,
                runtime::Context& context) override;

    private:
        VariableValue object_;
        std::string field_name_;
        std::unique_ptr<Statement> rv_;
    };

    //---------------------------class None--------------------------//
    // �������� None
    //---------------------------------------------------------------//
    class None : public Statement {
    public:
        runtime::ObjectHolder 
        Execute([[maybe_unused]] runtime::Closure& closure,
                [[maybe_unused]] runtime::Context& context) override {
            return {};
        }
    };

    //--------------------------class Print--------------------------//
    // �������� print
    //---------------------------------------------------------------//
    class Print : public Statement {
    public:
        // �������������� ������� print ��� ������ ��������
        // ��������� argument
        explicit Print(std::unique_ptr<Statement> argument);

        // �������������� ������� print ��� ������ ������ ��������
        // args
        explicit Print(std::vector<std::unique_ptr<Statement>> args);

        // �������������� ������� print ��� ������ �������� ����������
        // name
        static std::unique_ptr<Print> Variable(const std::string& name);

        // �� ����� ���������� ������� print ����� ������
        // �������������� � �����, ������������ ��
        // context.GetOutputStream()
        runtime::ObjectHolder 
        Execute(runtime::Closure& closure,
                runtime::Context& context) override;

    private:
        std::vector<std::unique_ptr<Statement>> args_;
    };

    //------------------------class MethodCall-----------------------//
    // �������� ����� object.method �� ������� ���������� args
    //---------------------------------------------------------------//
    class MethodCall : public Statement {
    public:
        MethodCall(std::unique_ptr<Statement> object,
                   std::string method,
                   std::vector<std::unique_ptr<Statement>> args);

        runtime::ObjectHolder
        Execute(runtime::Closure& closure,
                runtime::Context& context) override;

    private:
        std::unique_ptr<Statement> object_;
        std::string method_;
        std::vector<std::unique_ptr<Statement>> args_;
    };

    //-----------------------class NewInstance-----------------------//
    // ������ ����� ��������� ������ class_, ��������� ���
    // ������������ ����� ���������� args.
    // ���� � ������ ����������� ����� __init__ � �������� �����������
    // ����������, �� ��������� ������ �������� ��� ������
    // ������������ (���� ������� �� ����� �������������������):
    /*
    class Person:
      def set_name(name):
        self.name = name
    p = Person()
    # ���� name ����� ����� �������� ������ ����� ������ ������
    # set_name
    p.set_name("Ivan")
    */
    //---------------------------------------------------------------//
    class NewInstance : public Statement {
    public:
        explicit NewInstance(const runtime::Class& class_);

        NewInstance(const runtime::Class& class_,
                    std::vector<std::unique_ptr<Statement>> args);

        // ���������� ������, ���������� �������� ���� ClassInstance
        runtime::ObjectHolder
        Execute(runtime::Closure& closure,
                runtime::Context& context) override;

    private:
        const runtime::Class& class_ref_;
        std::vector<std::unique_ptr<Statement>> args_;
    };

    //----------------------class UnaryOperation---------------------//
    // ������� ����� ��� ������� ��������
    //---------------------------------------------------------------//
    class UnaryOperation : public Statement {
    public:
        explicit UnaryOperation(std::unique_ptr<Statement> argument)
            : argument_(std::move(argument))
        {}

    protected:
        std::unique_ptr<Statement> argument_;
    };

    //------------------------class Stringify------------------------//
    // �������� str, ������������ ��������� �������� ������ ���������
    //---------------------------------------------------------------//
    class Stringify : public UnaryOperation {
    public:
        using UnaryOperation::UnaryOperation;

        runtime::ObjectHolder
        Execute(runtime::Closure& closure,
                runtime::Context& context) override;

    };

    //---------------------class BinaryOperation---------------------//
    // ������������ ����� �������� �������� � ����������� lhs � rhs
    //---------------------------------------------------------------//
    class BinaryOperation : public Statement {
    public:
        BinaryOperation(std::unique_ptr<Statement> lhs,
                        std::unique_ptr<Statement> rhs)
            : lhs_(std::move(lhs))
            , rhs_(std::move(rhs))
        {}

    protected:
        std::unique_ptr<Statement> lhs_;
        std::unique_ptr<Statement> rhs_;
    };

    //---------------------------class Add---------------------------//
    // ���������� ��������� �������� + ��� ����������� lhs � rhs
    //---------------------------------------------------------------//
    class Add : public BinaryOperation {
    public:
        using BinaryOperation::BinaryOperation;

        // �������������� ��������:
        //  ����� + �����
        //  ������ + ������
        //  ������1 + ������2, ���� � ������1 - ����������������
        //  ����� � ������� _add__(rhs)
        // � ��������� ������ ��� ���������� �������������
        // runtime_error
        runtime::ObjectHolder
        Execute(runtime::Closure& closure,
                runtime::Context& context) override;

    };

    //---------------------------class Sub---------------------------//
    // ���������� ��������� ��������� ���������� lhs � rhs
    //---------------------------------------------------------------//
    class Sub : public BinaryOperation {
    public:
        using BinaryOperation::BinaryOperation;

        // �������������� ���������:
        //  ����� - �����
        // ���� lhs � rhs - �� �����, ������������� ����������
        // runtime_error
        runtime::ObjectHolder
        Execute(runtime::Closure& closure,
                runtime::Context& context) override;

    };

    //--------------------------class Mult---------------------------//
    // ���������� ��������� ��������� ���������� lhs � rhs
    //---------------------------------------------------------------//
    class Mult : public BinaryOperation {
    public:
        using BinaryOperation::BinaryOperation;

        // �������������� ���������:
        //  ����� * �����
        // ���� lhs � rhs - �� �����, ������������� ����������
        // runtime_error
        runtime::ObjectHolder
        Execute(runtime::Closure& closure,
                runtime::Context& context) override;

    };

    //---------------------------class Div---------------------------//
    // ���������� ��������� ������� ���������� lhs � rhs
    //---------------------------------------------------------------//
    class Div : public BinaryOperation {
    public:
        using BinaryOperation::BinaryOperation;

        // �������������� �������:
        //  ����� / �����
        // ���� lhs � rhs - �� �����, ������������� ����������
        // runtime_error
        // ���� rhs ����� 0, ������������� ���������� runtime_error
        runtime::ObjectHolder
        Execute(runtime::Closure& closure,
                runtime::Context& context) override;

    };

    //---------------------------class Or----------------------------//
    // ���������� ��������� ���������� ���������� �������� or ���
    // lhs � rhs
    //---------------------------------------------------------------//
    class Or : public BinaryOperation {
    public:
        using BinaryOperation::BinaryOperation;

        // �������� ��������� rhs �����������, ������ ���� ��������
        // lhs ����� ���������� � Bool ����� False
        runtime::ObjectHolder
        Execute(runtime::Closure& closure,
                runtime::Context& context) override;

    };

    //---------------------------class And---------------------------//
    // ���������� ��������� ���������� ���������� �������� and ���
    // lhs � rhs
    //---------------------------------------------------------------//
    class And : public BinaryOperation {
    public:
        using BinaryOperation::BinaryOperation;

        // �������� ��������� rhs �����������, ������ ���� ��������
        // lhs ����� ���������� � Bool ����� True
        runtime::ObjectHolder
        Execute(runtime::Closure& closure,
                runtime::Context& context) override;

    };

    //---------------------------class Not---------------------------//
    // ���������� ��������� ���������� ���������� �������� not ���
    // ������������ ���������� ��������
    //---------------------------------------------------------------//
    class Not : public UnaryOperation {
    public:
        using UnaryOperation::UnaryOperation;

        runtime::ObjectHolder
        Execute(runtime::Closure& closure,
                runtime::Context& context) override;

    };

    //-------------------------class Compound------------------------//
    // ��������� ���������� (��������: ���� ������, ���������� �����
    // if, ���� else)
    //---------------------------------------------------------------//
    class Compound : public Statement {
    public:
        // ������������ Compound �� ���������� ���������� ����
        // unique_ptr<Statement>
        template <typename... Args>
        explicit Compound(Args&&... args)
        {
            if constexpr (sizeof...(args) > 0) {
                CompoundImpl(std::move(args)...);
            }
        }

        // ��������� ��������� ���������� � ����� ���������
        // ����������
        void AddStatement(std::unique_ptr<Statement> stmt) {
            statements_.push_back(std::move(stmt));
        }

        // ��������������� ��������� ����������� ����������.
        // ���������� None
        runtime::ObjectHolder
        Execute(runtime::Closure& closure,
                runtime::Context& context) override;

    private:
        std::vector<std::unique_ptr<Statement>> statements_;

        template <typename T0, typename... Args>
        void CompoundImpl(T0 v0, Args... vs) {
            statements_.push_back(std::move(v0));
            if constexpr (sizeof...(vs) > 0) {
                CompoundImpl(std::move(vs)...);
            }
        }
    };

    //------------------------class MethodBody-----------------------//
    // ���� ������. ��� �������, �������� ��������� ����������
    //---------------------------------------------------------------//
    class MethodBody : public Statement {
    public:
        explicit MethodBody(std::unique_ptr<Statement>&& body);

        // ��������� ����������, ���������� � �������� body.
        // ���� ������ body ���� ��������� ���������� return, 
        // ���������� ��������� return
        // � ��������� ������ ���������� None
        runtime::ObjectHolder
        Execute(runtime::Closure& closure,
                runtime::Context& context) override;

    private:
        std::unique_ptr<Statement> body_;
    };

    //--------------------------class Return-------------------------//
    // ��������� ���������� return � ���������� statement
    //---------------------------------------------------------------//
    class Return : public Statement {
    public:
        explicit Return(std::unique_ptr<Statement> statement)
            : statement_(std::move(statement))
        {}

        // ������������� ���������� �������� ������. ����� ����������
        // ���������� return �����, ������ �������� ��� ���� ���������,
        // ������ ������� ��������� ���������� ��������� statement.
        runtime::ObjectHolder
        Execute(runtime::Closure& closure,
                runtime::Context& context) override;

    private:
        std::unique_ptr<Statement> statement_;
    };

    //---------------------class ClassDefinition---------------------//
    // ��������� �����
    //---------------------------------------------------------------//
    class ClassDefinition : public Statement {
    public:
        // �������������, ��� ObjectHolder �������� ������ ����
        // runtime::Class
        explicit ClassDefinition(runtime::ObjectHolder cls);

        // ������ ������ closure ����� ������, ����������� � ������
        // ������ � ���������, ���������� � �����������
        runtime::ObjectHolder
        Execute(runtime::Closure& closure,
                runtime::Context& context) override;

    private:
        runtime::ObjectHolder cls_;
    };

    //--------------------------class IfElse-------------------------//
    // ���������� if <condition> <if_body> else <else_body>
    //---------------------------------------------------------------//
    class IfElse : public Statement {
    public:
        // �������� else_body ����� ���� ����� nullptr
        IfElse(std::unique_ptr<Statement> condition,
               std::unique_ptr<Statement> if_body,
               std::unique_ptr<Statement> else_body);

        runtime::ObjectHolder
        Execute(runtime::Closure& closure,
                runtime::Context& context) override;

    private:
        std::unique_ptr<Statement> condition_;
        std::unique_ptr<Statement> if_body_;
        std::unique_ptr<Statement> else_body_;
    };

    //------------------------class Comparison-----------------------//
    // �������� ���������
    //---------------------------------------------------------------//
    class Comparison : public BinaryOperation {
    public:
        // Comparator ����� �������, ����������� ��������� ��������
        // ����������
        using Comparator = 
            std::function<bool(const runtime::ObjectHolder&,
                               const runtime::ObjectHolder&,
                               runtime::Context&)>;

        Comparison(Comparator cmp, std::unique_ptr<Statement> lhs,
                   std::unique_ptr<Statement> rhs);

        // ��������� �������� ��������� lhs � rhs � ����������
        // ��������� ������ comparator, ���������� � ����
        // runtime::Bool
        runtime::ObjectHolder
        Execute(runtime::Closure& closure,
                runtime::Context& context) override;

    private:
        Comparator cmp_;
    };

}  // namespace ast