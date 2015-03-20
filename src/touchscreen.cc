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
    NanCallback *callback;

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

    if (args[1]->IsUndefined()) {
        return NanThrowError("No callback function specified");
    }

    if (!args[1]->IsFunction()) {
        return NanThrowError("Second argument should be a function");
    }

    NanUtf8String *path = new NanUtf8String(args[0]);

    TouchInfo* touchInfo = new TouchInfo();
    touchInfo->fileDescriptor = open(**path, O_RDONLY);
    touchInfo->callback = new NanCallback(args[1].As<Function>());
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
    NanScope();

    TouchInfo* touchInfo = static_cast<TouchInfo*>(req->data);

    if (touchInfo->error) {
        Local<Value> err = Exception::Error(NanNew<String>(touchInfo->errorMessage.c_str()));

        const unsigned argc = 1;
        Local<Value> argv[argc] = { err };

        TryCatch try_catch;

        touchInfo->callback->Call(NanGetCurrentContext()->Global(), argc, argv);

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
                 Persistent<Object> o;
                 Local<Object> touch = NanNew(o);
                 touch->Set(NanNew<String>("x"), NanNew<Number>(touchInfo->x));
                 touch->Set(NanNew<String>("y"), NanNew<Number>(touchInfo->y));
                 touch->Set(NanNew<String>("pressure"), NanNew<Number>(touchInfo->pressure));
                 touch->Set(NanNew<String>("touch"), NanNew<Number>(touchInfo->inputEvents[i].value));
                 touch->Set(NanNew<String>("stop"), NanNew<Boolean>(touchInfo->stop));

                 const unsigned argc = 2;
                 Local<Value> argv[argc] = { NanNull(), NanNew(touch) };

                 TryCatch try_catch;

                 touchInfo->callback->Call(NanGetCurrentContext()->Global(), argc, argv);
                 touchInfo->stop = touch->Get(NanNew<String>("stop"))->BooleanValue();

                 if (try_catch.HasCaught()) {
                     FatalException(try_catch);
                 }
            }
        }

        if (touchInfo->stop) {
            //delete touchInfo->callback;
            delete touchInfo;
            delete req;
        } else {
            int status = uv_queue_work(uv_default_loop(), req, AsyncWork, (uv_after_work_cb)AsyncAfter);
            assert(status == 0);
        }
    }
}

void InitAll(Handle<Object> exports, Handle<Object> module) {
    NanScope();

    module->Set(NanNew("exports"),
      NanNew<FunctionTemplate>(Async)->GetFunction());
}

NODE_MODULE(ts, InitAll)
