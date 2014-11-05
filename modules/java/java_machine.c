#include "java_machine.h"
#include "syslog-ng.h"
#include "atomic.h"

struct _JavaVMSingleton
{
  GAtomicCounter ref_cnt;
  JavaVMOption options[2];
  JNIEnv *env;
  JavaVM *jvm;
  JavaVMInitArgs vm_args;
  GString *class_path;
  GList *class_path_list;
};

static JavaVMSingleton *g_jvm_s;

JavaVMSingleton *
java_machine_ref()
{
  if (g_jvm_s)
    {
      g_atomic_counter_inc(&g_jvm_s->ref_cnt);
      return g_jvm_s;
    }
  else
    {
      g_jvm_s = g_new0(JavaVMSingleton, 1);
      g_atomic_counter_set(&g_jvm_s->ref_cnt, 1);
      g_jvm_s->class_path = g_string_sized_new(1024);
      GString *default_class_path = g_string_new(module_path);
      g_string_append(default_class_path, "/SyslogNgInterface.jar");
      java_machine_add_class_path(g_jvm_s, default_class_path);
      return g_jvm_s;
    }
}

void
java_machine_unref(JavaVMSingleton *self)
{
  g_assert(self == g_jvm_s);
  if (g_atomic_counter_dec_and_test(&self->ref_cnt))
    {
      JavaVM jvm = *(self->jvm);
      g_string_free(self->class_path, TRUE);
      g_list_free(self->class_path_list);
      jvm->DestroyJavaVM(self->jvm);
      g_free(self);
      g_jvm_s = NULL;
    }
}

static void
__build_class_path(GList *class_path_list, GString *class_path)
{
  GList *element = class_path_list;
  while (element)
    {
      GString *cp = element->data;
      if (class_path->len > 0)
        {
          g_string_append_c(class_path, ':');
        }
      g_string_append_len(class_path, cp->str, cp->len);
      element = element->next;
    }
}

gboolean
java_machine_start(JavaVMSingleton* self, JNIEnv **env)
{
  g_assert(self == g_jvm_s);
  if (!self->jvm)
    {
      long status;
      __build_class_path(self->class_path_list, self->class_path);
      self->options[0].optionString = g_strdup_printf(
          "-Djava.class.path=%s", self->class_path->str);

      self->options[1].optionString = g_strdup_printf(
          "-Djava.library.path=%s", module_path);

      self->vm_args.version = JNI_VERSION_1_6;
      self->vm_args.nOptions = 2;
      self->vm_args.options = self->options;
      status = JNI_CreateJavaVM(&self->jvm, (void**) &self->env,
                                &self->vm_args);
      if (status == JNI_ERR) {
          return FALSE;
      }
    }
  *env = self->env;
  return TRUE;
}

void
java_machine_add_class_path(JavaVMSingleton* self, GString *class_path)
{
  g_assert(self == g_jvm_s);
  g_assert(!self->jvm);
  self->class_path_list = g_list_append(self->class_path_list, class_path);
}


void
java_machine_attach_thread(JavaVMSingleton* self, JNIEnv **penv)
{
  g_assert(self == g_jvm_s);
  (*(self->jvm))->AttachCurrentThread(self->jvm, (void **)penv, &self->vm_args);
}

void
java_machine_detach_thread(JavaVMSingleton* self)
{
  g_assert(self == g_jvm_s);
  (*(self->jvm))->DetachCurrentThread(self->jvm);
}

const JNIEnv *
java_machine_get_env(JavaVMSingleton* self)
{
  g_assert(self == g_jvm_s);
  return self->env;
}
