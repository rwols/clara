#include "PythonGILEnsurer.hpp"
#include "CancellableSyntaxOnlyAction.hpp"
#include "CancellableASTConsumer.hpp"
#include "Session.hpp"

using namespace Clara;

CancellableSyntaxOnlyAction::CancellableSyntaxOnlyAction(Session& owner)
: mOwner(owner)
{

}

std::unique_ptr<clang::ASTConsumer> CancellableSyntaxOnlyAction::CreateASTConsumer(
	clang::CompilerInstance &instance, 
	llvm::StringRef inFile)
{
	mOwner.report("Creating new ASTConsumer.");
	return std::make_unique<CancellableASTConsumer>(*this);
}


void CancellableSyntaxOnlyAction::cancel()
{
	std::unique_lock<std::mutex> lock(mCancelMutex);
	if (mConsumer == nullptr) return;
	mPleaseCancel = true;
	mCancelVar.wait(lock, [this]{ return mConsumer == nullptr; });
}
