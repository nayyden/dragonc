#include "Expressions.h"

#include "DragonContext.h"
#include "DragonSymbolTable.h"
#include <iostream>

namespace Dragonc
{

SymbolTable _SymbolTable;

Value * AddExpression::emitCode(IRBuilder< true >& builder, Module &module)
{
	return builder.CreateAdd(mRhs->emitCode(builder, module), mLhs->emitCode(builder, module));
}


Value * SubstractExpression::emitCode(IRBuilder< true >& builder, Module &module)
{
	return builder.CreateSub(mRhs->emitCode(builder, module), mLhs->emitCode(builder, module));
}


Value * MultiplyExpression::emitCode(IRBuilder< true >& builder, Module &module)
{
	return builder.CreateMul(mRhs->emitCode(builder, module), mLhs->emitCode(builder, module));
}


Value * DivideExpression::emitCode(IRBuilder< true >& builder, Module &module)
{
	Value* rexpr = builder.CreateCast(Instruction::UIToFP, mRhs->emitCode(builder, module), builder.getFloatTy());
	Value* lexpr = builder.CreateCast(Instruction::UIToFP, mLhs->emitCode(builder, module), builder.getFloatTy());
	return builder.CreateCast(Instruction::FPToUI, builder.CreateFDiv(rexpr, lexpr), builder.getInt32Ty());
}


Value * AssignmentExpression::emitCode(IRBuilder< true >& builder, Module &module)
{
	Value* rVal = mLhs->emitCode(builder, module);
	Value* lVal = mRhs->emitCode(builder, module);
	return builder.CreateStore(lVal, rVal);
}


Value* IdentifierDeclaration::emitCode(IRBuilder<> &builder, Module &module)
{
	if(!mValue)
	{
		mBuilder = &builder;
		mValue = builder.CreateAlloca(Type::getInt32Ty(getGlobalContext()), 0, mName);

		_SymbolTable.addSymbol(mName, this);
	}
	return mValue;
}


Value *IntegerValueExpression::emitCode(IRBuilder<>& builder, Module &module)
{
	return ConstantInt::get(getGlobalContext(), APInt(32, mValue));
}


Value* UseVariableExpression::emitCode(IRBuilder< true >& builder, Module &module)
{
	BaseExpression* e = _SymbolTable[mName];
	return e->getValue();
}

Value* UseVariableExpression::getValue()
{
	return _SymbolTable[mName]->getValue();
}

Value* FuncArgumentExpression::emitCode(IRBuilder< true >& builder, Module &module)
{
	return mArg;
}

Value *FunctionDeclExpr::emitCode(IRBuilder<>& builder, Module &module)
{
	if(DragonContext::get() != GLOBAL)
	{
		std::cout << "Function '" << mName << "'' can not be declared here";
		return 0;
	}

	Type* returnType = toLlvmType(builder, mReturnType);

	if(!returnType)
	{
		throw "Invalid return type for function " + mName;
	}

	llvm::FunctionType *funcType = 0;

	// Function has arguments?
	if(mArgList.size())
	{
		// Get function type having the following arguments
		std::vector<Type*> argList;
		for(VariableIterator it = mArgList.begin(); it != mArgList.end(); it++)
		{
			argList.push_back(toLlvmType(builder, it->type));
		}
		funcType = llvm::FunctionType::get(returnType, argList, false);
	}
	// Function taking no arguments
	else
	{
		funcType = llvm::FunctionType::get(returnType, false);
	}

	llvm::Function *funcPtr = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, mName, &module);

	// Give the arguments names
	if(mArgList.size())
	{
		unsigned i = 0;
		for (Function::arg_iterator AI = funcPtr->arg_begin(); i != mArgList.size(); ++AI, ++i)
		{
			AI->setName(mArgList[i].name);
			_SymbolTable.addSymbol(mArgList[i].name, new FuncArgumentExpression(AI));
		}
	}

	llvm::BasicBlock *entry = llvm::BasicBlock::Create(module.getContext(), "entrypoint", funcPtr);
	builder.SetInsertPoint(entry);

	_SymbolTable.addSymbol(mName, this);

	return funcPtr;
}

Value *PrintfInvocation::emitCode(IRBuilder< true >& builder, Module &module)
{
	Function* printf_proto = module.getFunction("printf");

	llvm::Constant *format_const = llvm::ConstantDataArray::getString(builder.getContext(), "%d\n");
	llvm::GlobalVariable *var = new llvm::GlobalVariable(module, llvm::ArrayType::get(llvm::IntegerType::get(builder.getContext(), 8), 4),
					 true, llvm::GlobalValue::PrivateLinkage, format_const, ".str");
	llvm::Constant *zero = llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(builder.getContext()));

	std::vector<llvm::Constant*> indices;
	indices.push_back(zero);
	indices.push_back(zero);
	llvm::Constant *var_ref = llvm::ConstantExpr::getGetElementPtr(var, indices);

	llvm::CallInst *call = builder.CreateCall2(printf_proto, var_ref, mParams->emitCode(builder, module));
	call->setTailCall(false);
	
	return call;
}


}
