#include <boost/python.hpp>
#include "Session.hpp"
#include "DiagnosticConsumer.hpp"
#include "Configuration.hpp"
#include "SessionOptions.hpp"
#include <clang/Tooling/CompilationDatabase.h>
#include <cassert>

struct CompletionResultListToPythonList
{
	static PyObject* convert(const std::vector<std::pair<std::string,std::string>>& from)
	{
		boost::python::list to;
		for (const auto& fromPair : from)
		{
			boost::python::list toPair;
			toPair.append(fromPair.first);
			toPair.append(fromPair.second);
			to.append(toPair);
		}

		return boost::python::incref(to.ptr());;
	}
};


struct PythonListToStdVectorOfStringsConverter
{
	PythonListToStdVectorOfStringsConverter()
	{
		using namespace boost::python;
		converter::registry::push_back(&convertible, &construct, type_id<std::vector<std::string>>());
	}

	// Determine if obj can be converted in a QString
	static void* convertible(PyObject* obj)
	{
		if (!PySequence_Check(obj) || !PyObject_HasAttrString(obj,"__len__")) return nullptr;
		else return obj;
	}

	// Convert obj into an std::vector<std::string>
	static void construct(PyObject* obj, boost::python::converter::rvalue_from_python_stage1_data* data)
	{
		using namespace boost::python;
		using StringVec = std::vector<std::string>;
		void* storage=((converter::rvalue_from_python_storage<StringVec>*)(data))->storage.bytes;
		new (storage) StringVec();
		auto v = (StringVec*)storage;
		int l = PySequence_Size(obj);
		v->reserve(l);
		for( int i = 0; i < l; ++i)
		{
			v->emplace_back(extract<std::string>(PySequence_GetItem(obj, i)));
		}
		data->convertible = storage;
	}
};

BOOST_PYTHON_MODULE(cpp)
{
	using namespace boost::python;

	PyEval_InitThreads();

	class_<Clara::DiagnosticConsumer>("DiagnosticConsumer")
		.def("beginSourceFile", &Clara::DiagnosticConsumer::beginSourceFile)
		.def("endSourceFile", &Clara::DiagnosticConsumer::EndSourceFile)
		.def("finish", &Clara::DiagnosticConsumer::finish)
		.def("handleDiagnostic", &Clara::DiagnosticConsumer::handleDiagnostic)
	;


	class_<Clara::SessionOptions>("SessionOptions")
		.def_readwrite("filename", &Clara::SessionOptions::filename)
		.def_readwrite("systemHeaders", &Clara::SessionOptions::systemHeaders)
		.def_readwrite("builtinHeaders", &Clara::SessionOptions::builtinHeaders)
		.def_readwrite("jsonCompileCommands", &Clara::SessionOptions::jsonCompileCommands)
		.def_readwrite("cxx11", &Clara::SessionOptions::cxx11, "Wether to enable C++11 dialect.")
		.def_readwrite("cxx14", &Clara::SessionOptions::cxx14, "Wether to enable C++14 dialect.")
		.def_readwrite("cxx1z", &Clara::SessionOptions::cxx1z, "Wether to enable C++1z dialect.")
		.def_readwrite("codeCompleteIncludeMacros", &Clara::SessionOptions::codeCompleteIncludeMacros)
		.def_readwrite("codeCompleteIncludeCodePatterns", &Clara::SessionOptions::codeCompleteIncludeCodePatterns)
		.def_readwrite("codeCompleteIncludeGlobals", &Clara::SessionOptions::codeCompleteIncludeGlobals)
		.def_readwrite("codeCompleteIncludeBriefComments", &Clara::SessionOptions::codeCompleteIncludeBriefComments)
	;

	class_<Clara::Session, boost::noncopyable>("Session", init<Clara::DiagnosticConsumer&, const std::string&>())
		.def(init<Clara::DiagnosticConsumer&, const std::string&, const std::string&>())
		.def(init<const std::string&>())
		.def(init<const std::string&, const std::string&>())
		.def(init<const Clara::SessionOptions&>())
		.def_readwrite("reporter", &Clara::Session::reporter)
		.def("codeComplete", &Clara::Session::codeComplete)
		.def("codeCompleteAsync", &Clara::Session::codeCompleteAsync)
		.def("cancelAsyncCompletion", &Clara::Session::cancelAsyncCompletion)
		.def("filename", &Clara::Session::getFilename, return_value_policy<copy_const_reference>())
	;

	// Converters

	PythonListToStdVectorOfStringsConverter();
	to_python_converter<std::vector<std::pair<std::string, std::string>>, CompletionResultListToPythonList>();
}