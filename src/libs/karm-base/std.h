#pragma once

#include <new>
#include <utility>

#include "_prelude.h"
#include <initializer_list>

#ifndef __ssize_t_defined

#    include <karm-meta/signess.h>

using ssize_t = Karm::Meta::MakeSigned<size_t>;

#endif
