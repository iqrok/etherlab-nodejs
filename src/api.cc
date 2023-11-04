#include <etherlab-helper.h>
#include <TimespecHelper.hpp>
#include <napi.h>

#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC 1000000000
#endif

static int8_t running_state = -1;
static uint32_t period_ns = 0;

/************************** Thread Safe Function *******************************
 *
 * https://napi.inspiredware.com/special-topics/thread-safe-functions.html
 *
 * ****************************************************************************/

struct TsfnContext {
	TsfnContext(Napi::Env env) : deferred(Napi::Promise::Deferred::New(env)) {};

	// Native Promise returned to JavaScript
	Napi::Promise::Deferred deferred;

	// Native thread
	std::thread nativeThread;

	Napi::ThreadSafeFunction tsfn;
};

void finalizer_cb(Napi::Env env, void *finalizeData, TsfnContext *context)
{
	// Join the thread
	context->nativeThread.join();

	// Resolve the Promise previously returned to JS via the js_create_thread method.
	context->deferred.Resolve(Napi::Boolean::New(env, true));
	delete context;
}

void thread_entry(TsfnContext *context) {
	auto routine_cb = [](Napi::Env env, Napi::Function js_cb,
		std::vector<EcatHelper::ecat_slave_entry_al>* domain_data) {

		size_t pd_size = domain_data->size();
		Napi::Value states
			= Napi::Number::New(env, EcatHelper::application_layer_states());
		Napi::Array array = Napi::Array::New(env, pd_size);

		for(size_t dmn_idx = 0; dmn_idx < pd_size; dmn_idx++){
			Napi::Object elem = Napi::Object::New(env);

			uint8_t bit_size = domain_data->at(dmn_idx).size;
			uint8_t is_signed = domain_data->at(dmn_idx).is_signed;

			elem.Set("position", Napi::Value::From(env, domain_data->at(dmn_idx).position));
			elem.Set("index", Napi::Value::From(env, (*domain_data).at(dmn_idx).index));
			elem.Set("subindex", Napi::Value::From(env, (*domain_data).at(dmn_idx).subindex));
			elem.Set("size", Napi::Value::From(env, bit_size));

			EcatHelper::ecat_value_al* value = &(domain_data->at(dmn_idx).value);
			switch(bit_size){

			case 1: {
				elem.Set("value", Napi::Number::New(env, value->u8 & 0x1));
			} break;

			case 8: {
				if(is_signed) {
					elem.Set("value", Napi::Number::New(env, value->i8));
				} else {
					elem.Set("value", Napi::Number::New(env, value->u8));
				}
			} break;

			case 16: {
				if(is_signed) {
					elem.Set("value", Napi::Number::New(env, value->i16));
				} else {
					elem.Set("value", Napi::Number::New(env, value->u16));
				}
			} break;

			case 32: {
				if(is_signed) {
					elem.Set("value", Napi::Number::New(env, value->i32));
				} else {
					elem.Set("value", Napi::Number::New(env, value->u32));
				}
			} break;

			}

			array[dmn_idx] = elem;
		}

		js_cb.Call({ array, states });
	};

	std::vector<EcatHelper::ecat_slave_entry_al>* domain_data;
	EcatHelper::attach_process_data(&domain_data);

	struct timespec wakeup_time;

	/* Set priority */
	struct sched_param param = {};
	param.sched_priority = sched_get_priority_max(SCHED_FIFO);

	if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
		perror("sched_setscheduler failed");
	}

	EcatHelper::prerun_routine();

	clock_gettime(CLOCK_MONOTONIC, &wakeup_time);
	wakeup_time.tv_sec += 1; /* start in future */
	wakeup_time.tv_nsec = 0;

	running_state = 1;

	while (1) {
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &wakeup_time, NULL);

		if (!running_state) {
			fprintf(stderr, "\nBreak Cyclic Process (running_state %d)\n",
				running_state);
			context->tsfn.Abort();
			break;
		}

		EcatHelper::main_routine();

		napi_status status = context->tsfn.BlockingCall(domain_data, routine_cb);

		if (status != napi_ok) {
			Napi::Error::Fatal(
					"thread_entry",
					"Napi::ThreadSafeNapi::Function.BlockingCall() failed"
				);
		}

		wakeup_time.tv_nsec += period_ns;
		Timespec::normalize_upper(&wakeup_time);
	}

	EcatHelper::postrun_routine();
}
/********************** End of Thread Safe Function ***************************/

Napi::Value js_thread_start(const Napi::CallbackInfo &info)
{
	Napi::Env env = info.Env();

	// Construct context data
	auto _ctx = new TsfnContext(env);

	// Create a new ThreadSafeFunction.
	_ctx->tsfn = Napi::ThreadSafeFunction::New(
			env, // Environment
			info[0].As<Napi::Function>(), // JS function from caller
			"EcatThread", // Resource name
			0, // Max queue size (0 = unlimited).
			1, // Initial thread count
			_ctx, // Context,
			finalizer_cb, // Finalizer
			(void *)nullptr	// Finalizer data
		);

	_ctx->nativeThread = std::thread(thread_entry, _ctx);

	return _ctx->deferred.Promise();
}

Napi::Value js_thread_stop(const Napi::CallbackInfo &info)
{
	Napi::Env env = info.Env();

	running_state = 0;

	return Napi::Number::New(env, running_state);
}

Napi::Value js_init(const Napi::CallbackInfo& info)
{
	Napi::Env env = info.Env();

	EcatHelper::init();

	return Napi::Boolean::New(env, true);
}

Napi::Value js_set_json_path(const Napi::CallbackInfo& info)
{
	Napi::Env env = info.Env();

	std::string json_path = info[0].As<Napi::String>();

	EcatHelper::set_json_path(json_path);

	return Napi::Boolean::New(env, true);
}

Napi::Value js_set_frequency(const Napi::CallbackInfo& info)
{
	Napi::Env env = info.Env();

	uint32_t frequency = info[0].As<Napi::Number>();

	EcatHelper::set_frequency(frequency);
	period_ns = EcatHelper::get_period();

	return Napi::Number::New(env, period_ns);
}

Napi::Value js_domain_write(const Napi::CallbackInfo& info)
{
	Napi::Env env = info.Env();

	EcatHelper::ecat_pos_al pos = info[0].As<Napi::Number>().Uint32Value();
	EcatHelper::ecat_index_al index = info[1].As<Napi::Number>().Uint32Value();
	EcatHelper::ecat_sub_al subindex = info[2].As<Napi::Number>().Uint32Value();
	EcatHelper::ecat_value_al value = {};
	value.u32 = info[3].As<Napi::Number>().Uint32Value();

	if(EcatHelper::domain_write(pos, index, subindex, value)){
		return Napi::Boolean::New(env, false);
	}

	return Napi::Boolean::New(env, true);
}

Napi::Value js_domain_read(const Napi::CallbackInfo& info)
{
	Napi::Env env = info.Env();

	EcatHelper::ecat_pos_al pos = info[0].As<Napi::Number>().Uint32Value();
	EcatHelper::ecat_index_al index = info[1].As<Napi::Number>().Uint32Value();
	EcatHelper::ecat_sub_al subindex = info[2].As<Napi::Number>().Uint32Value();
	EcatHelper::ecat_value_al value = {};

	if(EcatHelper::domain_read(pos, index, subindex, &value)){
		return env.Undefined();
	}

	return Napi::Number::New(env, value.u32);
}

Napi::Value js_sdo_read(const Napi::CallbackInfo& info)
{
	Napi::Env env = info.Env();

	EcatHelper::ecat_pos_al pos = info[0].As<Napi::Number>().Uint32Value();
	EcatHelper::ecat_index_al index = info[1].As<Napi::Number>().Uint32Value();
	EcatHelper::ecat_sub_al subindex = info[2].As<Napi::Number>().Uint32Value();
	size_t size = info[3].As<Napi::Number>().Uint32Value();
	size_t result_size = 0;
	EcatHelper::ecat_value_al value = {};

	if(EcatHelper::sdo_upload(
		pos, index, subindex, size, &result_size, value.bytes)){

		return env.Undefined();
	}

	return Napi::Number::New(env, value.u32);
}

Napi::Value js_sdo_write(const Napi::CallbackInfo& info)
{
	Napi::Env env = info.Env();

	EcatHelper::ecat_pos_al pos = info[0].As<Napi::Number>().Uint32Value();
	EcatHelper::ecat_index_al index = info[1].As<Napi::Number>().Uint32Value();
	EcatHelper::ecat_sub_al subindex = info[2].As<Napi::Number>().Uint32Value();
	size_t size = info[3].As<Napi::Number>().Uint32Value();
	EcatHelper::ecat_value_al value = {};

	if(EcatHelper::sdo_download(pos, index, subindex, size, value.bytes)){
		return Napi::Boolean::New(env, false);
	}

	return Napi::Boolean::New(env, true);
}

Napi::Value js_al_states(const Napi::CallbackInfo& info)
{
	Napi::Env env = info.Env();

	return Napi::Number::New(env, EcatHelper::application_layer_states());
}

Napi::Object InitNodeApi(Napi::Env env, Napi::Object exports)
{
	exports.Set(Napi::String::New(env, "setFrequency"), Napi::Function::New(env, js_set_frequency));
	exports.Set(Napi::String::New(env, "init"), Napi::Function::New(env, js_init));
	exports.Set(Napi::String::New(env, "setJSON"), Napi::Function::New(env, js_set_json_path));
	exports.Set(Napi::String::New(env, "sdoWrite"), Napi::Function::New(env, js_sdo_write));
	exports.Set(Napi::String::New(env, "sdoRead"), Napi::Function::New(env, js_sdo_read));
	exports.Set(Napi::String::New(env, "domainWrite"), Napi::Function::New(env, js_domain_write));
	exports.Set(Napi::String::New(env, "domainRead"), Napi::Function::New(env, js_domain_read));
	exports.Set(Napi::String::New(env, "start"), Napi::Function::New(env, js_thread_start));
	exports.Set(Napi::String::New(env, "stop"), Napi::Function::New(env, js_thread_stop));
	exports.Set(Napi::String::New(env, "getMasterState"), Napi::Function::New(env, js_al_states));

	return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, InitNodeApi);
