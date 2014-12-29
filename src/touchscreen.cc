#include <linux/input.h>

#include <v8.h>
#include <node.h>
#include <nan.h>

using namespace v8;
using namespace node;

NAN_METHOD(Async);
void AsyncWork(uv_work_t* req);
void AsyncAfter(uv_work_t* req);

struct TouchInfo {
    int fileDescriptor;
    Persistent<Function> callback;

    bool error;
    std::string errorMessage;

    struct input_event inputEvents[64];
    int read;

    int x;
    int y;
    int pressure;

    bool stop;
};

NAN_METHOD(Async) {
    NanScope();

    if (args[0]->IsUndefined()) {
        return NanThrowError("No parameters specified");
    }

    if (!args[0]->IsString()) {
        return NanThrowError("First parameter should be a string");
    }

    v8::String::Utf8Value path(args[0]->ToString());
    std::string _path = std::string(*path);

    if (args[1]->IsUndefined()) {
        return NanThrowError("No callback function specified");
    }

    if (!args[1]->IsFunction()) {
        return NanThrowError("Second argument should be a function");
    }

    Local<Function> callback = Local<Function>::Cast(args[1]);

    TouchInfo* touchInfo = new TouchInfo();
    touchInfo->fileDescriptor = open(_path.c_str(), O_RDONLY);
    touchInfo->callback = Persistent<Function>::New(callback);
    touchInfo->error = false;
    touchInfo->stop = false;

    uv_work_t *req = new uv_work_t();
    req->data = touchInfo;

    int status = uv_queue_work(uv_default_loop(), req, AsyncWork, (uv_after_work_cb)AsyncAfter);

    assert(status == 0);

    NanReturnUndefined();
}

void AsyncWork(uv_work_t* req) {
    TouchInfo* touchInfo = static_cast<TouchInfo*>(req->data);

    touchInfo->read = read(touchInfo->fileDescriptor, touchInfo->inputEvents, sizeof(struct input_event) * 64);

    if (touchInfo->read < (int) sizeof(struct input_event)) {
        touchInfo->error = true;
        touchInfo->errorMessage = "Read problem";
    }
}

void AsyncAfter(uv_work_t* req) {
    HandleScope scope;

    TouchInfo* touchInfo = static_cast<TouchInfo*>(req->data);

    if (touchInfo->error) {
        Local<Value> err = Exception::Error(String::New(touchInfo->errorMessage.c_str()));

        const unsigned argc = 1;
        Local<Value> argv[argc] = { err };

        TryCatch try_catch;

        touchInfo->callback->Call(Context::GetCurrent()->Global(), argc, argv);

        if (try_catch.HasCaught()) {
            FatalException(try_catch);
        }
    } else {
        for (unsigned i = 0; i < touchInfo->read / sizeof(struct input_event); i++) {
            if (touchInfo->inputEvents[i].type == EV_SYN) {
            } else if (touchInfo->inputEvents[i].type == EV_ABS && (touchInfo->inputEvents[i].code == ABS_Y)) {
                touchInfo->x = touchInfo->inputEvents[i].value;
            } else if (touchInfo->inputEvents[i].type == EV_ABS && (touchInfo->inputEvents[i].code == ABS_X)) {
                touchInfo->y = touchInfo->inputEvents[i].value;
            } else if (touchInfo->inputEvents[i].type == EV_ABS && (touchInfo->inputEvents[i].code == ABS_PRESSURE)) {
                touchInfo->pressure = touchInfo->inputEvents[i].value;
            } else if (touchInfo->inputEvents[i].type == EV_KEY && (touchInfo->inputEvents[i].code == BTN_TOUCH)) {
                 Local<Object> touch = Object::New();
                 touch->Set(String::NewSymbol("x"), Number::New(touchInfo->x));
                 touch->Set(String::NewSymbol("y"), Number::New(touchInfo->y));
                 touch->Set(String::NewSymbol("pressure"), Number::New(touchInfo->pressure));
                 touch->Set(String::NewSymbol("touch"), Number::New(touchInfo->inputEvents[i].value));
                 touch->Set(String::NewSymbol("stop"), Boolean::New(touchInfo->stop));

                 const unsigned argc = 2;
                 Local<Value> argv[argc] = { Local<Value>::New(Null()), Local<Value>::New(touch) };

                 TryCatch try_catch;

                 touchInfo->callback->Call(Context::GetCurrent()->Global(), argc, argv);
                 touchInfo->stop = touch->Get(String::NewSymbol("stop"))->BooleanValue();

                 if (try_catch.HasCaught()) {
                     FatalException(try_catch);
                 }
            }
        }

        if (touchInfo->stop) {
            touchInfo->callback.Dispose();
            delete touchInfo;
            delete req;
        } else {
            int status = uv_queue_work(uv_default_loop(), req, AsyncWork, (uv_after_work_cb)AsyncAfter);
            assert(status == 0);
        }
    }
}

void InitAll(Handle<Object> exports, Handle<Object> module) {
    module->Set(String::NewSymbol("exports"),
      FunctionTemplate::New(Async)->GetFunction());
}

NODE_MODULE(ts, InitAll)
