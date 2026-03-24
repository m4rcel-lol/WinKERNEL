#pragma once

#include <limine.h>

/* Single framebuffer request for the whole kernel — Limine panics if this ID
   appears more than once in .limine_requests. */
extern volatile struct limine_framebuffer_request g_LimineFramebufferRequest;
