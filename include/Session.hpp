#pragma once

#include "SessionOptions.hpp"
#include "CancellableSyntaxOnlyAction.hpp"
#include "DiagnosticConsumer.hpp"
#include "CodeCompleteConsumer.hpp"

#include <clang/Frontend/PCHContainerOperations.h>

#include <thread>

namespace Clara {

class DiagnosticConsumer; // forward declaration.

/**
 * @brief      Encapsulates a single file session.
 */
class Session
{
public:

	/**
	 * @brief      Constructs a new session.
	 *
	 * @param[in]  options  The options
	 */
	Session(const SessionOptions& options);

	/**
	 * @brief      Given a buffer that represents the unsaved file contents of
	 *             the filename that was passed into the constructor, and given
	 *             a row and column number, perform code completion at that row
	 *             and column. The result can be passed immediately as-is to
	 *             Sublime Text.
	 *
	 * @param[in]  unsavedBuffer  The buffer that is not yet saved to disk.
	 * @param[in]  row            Clang rows are 1-based, not 0-based.
	 * @param[in]  column         Clang columns are 1-based, not 0-based.
	 *
	 * @return     A python list which consists of pairs of strings.
	 */
	std::vector<std::pair<std::string, std::string>> codeComplete(const char* unsavedBuffer, int row, int column);

	/**
	 * @brief      Asynchronous version of Session::codeComplete.
	 *
	 * @param[in]  viewID         The view ID associated to the Sublime View
	 * @param[in]  unsavedBuffer  The buffer that is not yet saved to disk.
	 * @param[in]  row            Clang rows are 1-based, not 0-based.
	 * @param[in]  column         Clang columns are 1-based, not 0-based.
	 * @param[in]  callback       A callable python object that receives a list of string pairs.
	 */
	void codeCompleteAsync(const int viewID, std::string unsavedBuffer, int row, int column, pybind11::object callback);


	/**
	 * @brief      Cancels the in-flight completion that is occurring in a
	 *             background thread.
	 */
	void cancelAsyncCompletion();

	/**
	 * @brief      Returns the filename for the current session.
	 *
	 * @return     The filename for the current session.
	 */
	const std::string& getFilename() const noexcept;

	void reparse(const int viewID, pybind11::object reparseCallback);

	void report(const char* message) const;

private:

	friend class CancellableSyntaxOnlyAction;
	friend class CodeCompleteConsumer;

	clang::DiagnosticConsumer* createDiagnosticConsumer() const;
	void loadFromOptions(clang::CompilerInstance& instance) const;
	clang::CompilerInvocation* makeInvocation() const;
	void dump();
	std::vector<std::pair<std::string, std::string>> codeCompleteImpl(const char*, int, int);
	void codeCompletePrepare(clang::CompilerInstance& instance, const char* unsavedBuffer, int row, int column) const;
	void fillInvocationWithStandardHeaderPaths(clang::CompilerInvocation* invocation) const;
	void resetDiagnosticsEngine();
	clang::CompilerInvocation* createInvocationFromOptions();
	
	clang::SmallVector<clang::StoredDiagnostic, 8> mStoredDiags; // ugly hack, wait for clang devs to fix this
	clang::SmallVector<const llvm::MemoryBuffer*, 1> mOwnedBuffers; // ugly hack, wait for clang devs to fix this
	std::shared_ptr<clang::PCHContainerOperations> mPchOps = std::make_shared<clang::PCHContainerOperations>();
	SessionOptions mOptions;
	clang::LangOptions mLangOpts;
	clang::FileSystemOptions mFileOpts;
	clang::IntrusiveRefCntPtr<clang::FileManager> mFileMgr;
	clang::DiagnosticIDs mDiagIds;
	clang::IntrusiveRefCntPtr<clang::DiagnosticOptions> mDiagOpts;
	Clara::DiagnosticConsumer mDiagConsumer;
	clang::IntrusiveRefCntPtr<clang::DiagnosticsEngine> mDiags;
	clang::IntrusiveRefCntPtr<clang::SourceManager> mSourceMgr;
	std::unique_ptr<Clara::CodeCompleteConsumer> mCodeCompleteConsumer;
	std::unique_ptr<clang::ASTUnit> mUnit;

	std::mutex mMethodMutex;
};

} // namespace Clara