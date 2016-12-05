#include "CancellableASTConsumer.hpp"
#include "CancellableSyntaxOnlyAction.hpp"
#include "CancelException.hpp"
#include <clang/AST/DeclGroup.h>
#include <mutex>

namespace Clara {

CancellableASTConsumer::CancellableASTConsumer(CancellableSyntaxOnlyAction& creator)
: mCreator(creator)
{
	std::lock_guard<std::mutex> lock(mCreator.mCancelMutex);
	mCreator.mConsumer = this;
}

CancellableASTConsumer::~CancellableASTConsumer() noexcept
{
	std::unique_lock<std::mutex> lock(mCreator.mCancelMutex);
	mCreator.mConsumer = nullptr;
	mCreator.mPleaseCancel = false;
	lock.unlock();
	mCreator.mCancelVar.notify_all();
}

bool CancellableASTConsumer::HandleTopLevelDecl(clang::DeclGroupRef declGroup)
{
	// std::lock_guard<std::mutex> lock(mCreator.mCancelMutex);
	// std::unique_lock<std::mutex> lock(mCreator.mCancelMutex);
	// if (mCreator.mPleaseCancel)
	// {
	// 	// lock.unlock();
	// 	throw CancelException();
	// }
	// lock.unlock();
	std::unique_lock<std::mutex> lock(mCreator.mCancelMutex);
	if (mCreator.mPleaseCancel)
	{
		throw CancelException();
	}
	lock.unlock();
	return ASTConsumer::HandleTopLevelDecl(declGroup);
}

} // namespace Clara