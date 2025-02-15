#include "daScript/daScript.h"
#include <iostream>

using namespace das;

// Скрипт
const char* tutorial_text = R""""(
require CustomModule

[export]
def mod_data(var value:CustomData)
    print("das: mod_data({value})\n")
    print("mod_data in: {value}\n")
    value.number += 10
    print("mod_data out: {value}\n")
    custom_data_fn(value)
    void_cd_fn(value)
    void_void_fn()
    var res: int
    res = int_void_fn()
    print("das res:{res}\n")
    res = value.get_num
    print("das res:{res}\n")
    value |> print_num
    unsafe
        var ttt:CustomData
        ttt.number = 12
        ttt |> print_num
    value |> set_num
    value |> print_num

[export]
def get_data()
    print("das: get_data()\n")
    let value = new [[CustomModule::CustomData() number = 98]]
    return value

)"""";

// Кастомная структура
struct CustomData
{
	int number = 0;
	void print_num() { std::cout << "!!! print_num called! [number is:" << std::to_string(number) << "]" << std::endl; }
	void set_num(int value)
	{
        std::cout << "[set_num(" << std::to_string(value) << ")]" << std::endl;
		number = value;
	}
	int get_num()
	{
		std::cout << "[get_num()]" << std::endl;
		return number;
	}
};
MAKE_TYPE_FACTORY(CustomData, CustomData);
struct CustomDataAnnotation : public ManagedStructureAnnotation<CustomData>
{
	CustomDataAnnotation(ModuleLibrary& ml) :
		ManagedStructureAnnotation("CustomData", ml)
	{
		addField<DAS_BIND_MANAGED_FIELD(number)>("number");
		//addProperty<DAS_BIND_MANAGED_PROP(print_num)>("print_num", "print_num");
		//addProperty<DAS_BIND_MANAGED_PROP(set_num)>("set_num", "set_num");
		addProperty<DAS_BIND_MANAGED_PROP(get_num)>("get_num", "get_num");
	}
};

// Функция прокинутая в скрипт. Принемает и возвращает кастомную структуру
CustomData custom_data_fn(CustomData& value)
{
	// LOG_MSG("CustomData number:" << value.number);
	std::cout << "custom_data_fn called! number:" << std::to_string(value.number) << std::endl;
	return CustomData();
}

// Функция прокинутая в скрипт. Принемает кастомную структуру
void void_cd_fn(CustomData& value)
{
	// LOG_MSG("CustomData number:" << value.number);
	std::cout << "void_cd_fn called! number:" << std::to_string(value.number) << std::endl;
	return;
}

void void_void_fn()
{
	std::cout << "void_void_fn called!" << std::endl;
}

int int_void_fn()
{
	std::cout << "int_void_fn called!" << std::endl;
	return 10;
}

// Модуль через который будут прокинуты структуры и функции
class Module_Tutorial02 : public Module
{
  public:
	Module_Tutorial02() :
		Module("CustomModule")
	{ // module name, when used from das file
		ModuleLibrary lib(this);
		lib.addBuiltInModule();

		addAnnotation(make_smart<CustomDataAnnotation>(lib));

		addExtern<DAS_BIND_FUN(custom_data_fn), SimNode_ExtFuncCallAndCopyOrMove>(*this, lib, "custom_data_fn", SideEffects::accessExternal);
		addExtern<DAS_BIND_FUN(void_cd_fn), SimNode_ExtFuncCall>(*this, lib, "void_cd_fn", SideEffects::accessExternal);
		addExtern<DAS_BIND_FUN(void_void_fn), SimNode_ExtFuncCall>(*this, lib, "void_void_fn", SideEffects::invoke);
		addExtern<DAS_BIND_FUN(int_void_fn), SimNode_ExtFuncCall>(*this, lib, "int_void_fn", SideEffects::none);

        using print_method = DAS_CALL_MEMBER(CustomData::print_num);
		addExtern<DAS_CALL_METHOD(print_method)>(*this, lib, "print_num", SideEffects::invoke, DAS_CALL_MEMBER_CPP(CustomData::print_num));
			//->args({"custom_data"});

        using set_method = DAS_CALL_MEMBER(CustomData::set_num);
		addExtern<DAS_CALL_METHOD(set_method)>(*this, lib, "set_num", SideEffects::modifyExternal, DAS_CALL_MEMBER_CPP(CustomData::set_num));
			//->args({"custom_data", "value"});
	}
};
REGISTER_MODULE(Module_Tutorial02)

void tutorial()
{
	// Готовим контекст дасланга
	TextPrinter tout;								   // output stream for all compiler messages (stdout. for stringstream use TextWriter)
	ModuleGroup dummyLibGroup;						   // module group for compiled program
	auto		fAccess	 = make_smart<FsFileAccess>(); // default file access
	auto		fileInfo = make_unique<TextFileInfo>(tutorial_text, uint32_t(strlen(tutorial_text)), false);
	fAccess->setFileInfo("dummy.das", das::move(fileInfo));
	// compile program
	auto program = compileDaScript("dummy.das", fAccess, tout, dummyLibGroup);
	if (program->failed())
	{
		// if compilation failed, report errors
		tout << "failed to compile\n";
		for (auto& err : program->errors)
		{
			tout << reportError(err.at, err.what, err.extra, err.fixme, err.cerr);
		}
		return;
	}
	// create daScript context
	Context ctx(program->getContextStackSize());
	if (!program->simulate(ctx, tout))
	{
		// if interpretation failed, report errors
		tout << "failed to simulate\n";
		for (auto& err : program->errors)
		{
			tout << reportError(err.at, err.what, err.extra, err.fixme, err.cerr);
		}
		return;
	}

	// Функция написанная в скрипте которая изменяет данные
	auto fnModData = ctx.findFunction("mod_data");
	if (!fnModData)
	{
		tout << "function 'mod_data' not found\n";
		return;
	}
	static CustomData c_data{.number = 100};
	//LOG_MSG("C++ pre dl  c_data.number = " << c_data.number);
	auto das_c_data = cast<CustomData*>::from(&c_data);
	// Вызов
	auto result = ctx.evalWithCatch(fnModData, &das_c_data);
	if (auto ex = ctx.getException())
	{
		tout << "exception: " << ex << "\n";
		return;
	}
	//LOG_MSG("C++ post dl c_data.number = " << c_data.number << "\n\n\n");

	// Функция написанная в скрипте которая возвращает кастомные данные
	auto fnGetData = ctx.findFunction("get_data");
	if (!fnGetData)
	{
		tout << "function 'get_data' not found\n";
		return;
	}
	// Вызов
	auto g_data = ctx.evalWithCatch(fnGetData, nullptr);
	auto cd		= cast<CustomData*>::to(g_data);
	//LOG_MSG("cd" << cd->number);
	std::cout << "cd is: " << std::to_string(cd->number) << std::endl;
	if (auto ex = ctx.getException())
	{
		tout << "exception: " << ex << "\n";
		return;
	}
}

int main( int, char * [] ) {
	// request all da-script built in modules
	NEED_ALL_DEFAULT_MODULES;
	// request our custom module
	NEED_MODULE(Module_Tutorial02);
	// Initialize modules
	Module::Initialize();
	// run the tutorial
	tutorial();
	// shut-down daScript, free all memory
	Module::Shutdown();

	return 0;
}
