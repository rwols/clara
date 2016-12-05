#include "Session.hpp"
#include "CodeCompleteConsumer.hpp"
#include "StringException.hpp"
#include "CancelException.hpp"
#include "DiagnosticConsumer.hpp"
#include "SessionOptions.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <clang/Basic/TargetInfo.h>
#include <clang/Sema/CodeCompleteConsumer.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/HeaderSearch.h>
#include <clang/Frontend/FrontendActions.h>
#include <pybind11/stl.h>


#define DEBUG_PRINT llvm::errs() << __FILE__ << ':' << __LINE__ << '\n'

namespace Clara {

class SimplePrinter : public DiagnosticConsumer
{
public:
	SimplePrinter(Session& owner) : mOwner(owner) {}
	~SimplePrinter() override = default;
	void handleNote(const std::string& filename, int row, int column, const std::string& message) override
	{
		reportMsg("NOTE   ", filename, row, column, message);
	}
	void handleRemark(const std::string& filename, int row, int column, const std::string& message) override
	{
		reportMsg("REMARK ", filename, row, column, message);
	}
	void handleWarning(const std::string& filename, int row, int column, const std::string& message) override
	{
		reportMsg("WARNING", filename, row, column, message);
	}
	void handleError(const std::string& filename, int row, int column, const std::string& message) override
	{
		reportMsg("ERROR  ", filename, row, column, message);
	}
	void handleFatalError(const std::string& filename, int row, int column, const std::string& message) override
	{
		reportMsg("FATAL  ", filename, row, column, message);
	}
private:
	void reportMsg(const char* prefix, const std::string& filename, int row, int column, const std::string& message)
	{
		std::string msg(prefix);
		msg.append(": ");
		if (!filename.empty())
		{
			msg.append(filename);
			msg.append(":").append(std::to_string(row));
			msg.append(":").append(std::to_string(column));
			msg.append(": ");
		}
		msg.append(message);
		mOwner.report(msg.c_str());
	}
	Session& mOwner;
};

Session::Session(const SessionOptions& options)
: mOptions(options)
{
	mOptions.diagnosticConsumer = std::make_shared<SimplePrinter>(*this);
}

clang::CompilerInvocation* Session::makeInvocation() const
{
	using namespace clang;
	auto invocation = new CompilerInvocation();
	invocation->TargetOpts->Triple = llvm::sys::getDefaultTargetTriple();
	invocation->setLangDefaults(
		*invocation->getLangOpts(), 
		IK_CXX, 
		llvm::Triple(invocation->TargetOpts->Triple), 
		invocation->getPreprocessorOpts(), 
		mOptions.languageStandard);

	auto& frontendOpts = invocation->getFrontendOpts();
	frontendOpts.Inputs.emplace_back
	(
		getFilename(),
		FrontendOptions::getInputKindForExtension(getFilename())
	);

	fillInvocationWithStandardHeaderPaths(invocation);

	return invocation;
}

void Session::fillInvocationWithStandardHeaderPaths(clang::CompilerInvocation* invocation) const
{
	auto& headerSearchOpts = invocation->getHeaderSearchOpts();

	#ifdef PRINT_HEADER_SEARCH_PATHS
		headerSearchOpts.Verbose = true;
	#else
		headerSearchOpts.Verbose = false;
	#endif

	headerSearchOpts.UseBuiltinIncludes = true;
	headerSearchOpts.UseStandardSystemIncludes = true;
	headerSearchOpts.UseStandardCXXIncludes = true;
	headerSearchOpts.ResourceDir = mOptions.builtinHeaders;

	for (const auto& systemHeader : mOptions.systemHeaders)
	{
		headerSearchOpts.AddPath(systemHeader, clang::frontend::System, false, false);
	}
}

void Session::loadFromOptions(clang::CompilerInstance& instance) const
{
	using namespace clang;
	using namespace clang::tooling;

	CompilerInvocation* invocation;

	if (!mOptions.jsonCompileCommands.empty())
	{
		report("Attemping to load JSON compilation database.");
		std::string errorMsg;
		auto compdb = CompilationDatabase::autoDetectFromDirectory(mOptions.jsonCompileCommands, errorMsg);
		if (compdb)
		{
			auto compileCommands = compdb->getCompileCommands(getFilename());
			if (!compileCommands.empty())
			{
				std::vector<const char*> cstrings;
				for (const auto& compileCommand : compileCommands)
				{
					// Skip first argument, because that's the path to the compiler.
					for (std::size_t i = 1; i < compileCommand.CommandLine.size(); ++i)
					{
						const auto& str = compileCommand.CommandLine[i];
						if (str.find("-o") != std::string::npos)
						{
							++i;
						}
						else
						{
							cstrings.push_back(str.c_str());
							// report(cstrings.back());
						}
					}
				}
				auto& diagnostics = instance.getDiagnostics();
				diagnostics.setClient(mOptions.diagnosticConsumer.get(), false);
				invocation = createInvocationFromCommandLine(cstrings, &diagnostics);
				if (invocation)
				{
					invocation->getFileSystemOpts().WorkingDir = compileCommands[0].Directory;
					fillInvocationWithStandardHeaderPaths(invocation);
					report("Succesfully loaded compile commands.");
				}
				else
				{
					report("Compiler invocation failed.");
					invocation = makeInvocation();
				}
			}
			else
			{
				report("Could not find compile commands.");
				invocation = makeInvocation();
			}
		}
		else
		{
			report(errorMsg.c_str());
			invocation = makeInvocation();
		}
	}
	else
	{
		report("No JSON compilation database specified.");
		invocation = makeInvocation();
	}

	instance.setInvocation(invocation);
}

void Session::codeCompletePrepare(clang::CompilerInstance& instance, const char* unsavedBuffer, int row, int column) const
{
	using namespace clang;
	loadFromOptions(instance);
	instance.setSourceManager(nullptr);
	instance.createDiagnostics();
	auto& frontendOptions = instance.getFrontendOpts();
	frontendOptions.CodeCompletionAt.FileName = getFilename();
	frontendOptions.CodeCompletionAt.Line = row;
	frontendOptions.CodeCompletionAt.Column = column;
	llvm::MemoryBufferRef bufferAsRef(unsavedBuffer, getFilename());
	auto memBuffer = llvm::MemoryBuffer::getMemBuffer(bufferAsRef);
	auto& preprocessorOptions = instance.getPreprocessorOpts();
	for (auto& remappedFile : preprocessorOptions.RemappedFileBuffers)
	{
		delete remappedFile.second;
	}
	preprocessorOptions.RemappedFileBuffers.clear();
	preprocessorOptions.RemappedFileBuffers.emplace_back(getFilename(), memBuffer.release());
}

std::vector<std::pair<std::string, std::string>> Session::codeComplete(const char* unsavedBuffer, int row, int column) const
{
	using namespace clang;
	using namespace clang::frontend;
	pybind11::gil_scoped_release release;
	CompilerInstance instance;
	instance.createDiagnostics();
	CodeCompleteOptions codeCompleteOptions;
	instance.setCodeCompletionConsumer(new Clara::CodeCompleteConsumer(codeCompleteOptions, *this));
	codeCompletePrepare(instance, unsavedBuffer, row, column);
	std::vector<std::pair<std::string, std::string>> result;
	SyntaxOnlyAction action;
	if (instance.ExecuteAction(action))
	{
		auto consumer = static_cast<Clara::CodeCompleteConsumer*>(&instance.getCodeCompletionConsumer());
		consumer->moveResult(result);
	}
	else
	{
		report("Completion run FAILED.");
	}
	return result;
}

void Session::codeCompleteAsync(std::string unsavedBuffer, int row, int column, pybind11::object callback) const
{
	using namespace clang;
	using namespace clang::frontend;
	
	std::thread task( [this, row, column, callback, unsavedBuffer{std::move(unsavedBuffer)} ] () -> void
	{
		CompilerInstance instance;
		instance.createDiagnostics();
		CodeCompleteOptions codeCompleteOptions;
		instance.setCodeCompletionConsumer(new Clara::CodeCompleteConsumer(codeCompleteOptions, *this));
		codeCompletePrepare(instance, unsavedBuffer.c_str(), row, column);
		std::vector<std::pair<std::string, std::string>> result;
		SyntaxOnlyAction action;
		if (instance.ExecuteAction(action))
		{
			auto consumer = static_cast<Clara::CodeCompleteConsumer*>(&instance.getCodeCompletionConsumer());
			consumer->moveResult(result);
			pybind11::gil_scoped_acquire pythonLock;
			callback(row, column, result);
		}
	});
	task.detach();
}

const std::string& Session::getFilename() const noexcept
{
	return mOptions.filename;
}

void Session::report(const char* message) const
{
	pybind11::gil_scoped_acquire lock;
	if (message != nullptr && mOptions.logCallback != pybind11::object())
	{
		// yeah this is horrible.
		const_cast<Session*>(this)->mOptions.logCallback(message);
	}
}

Session::~Session()
{
}

} // namespace Clara