/* Copyright (c) 2017-2023 Hans-Kristian Arntzen
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "application.hpp"
#include "global_managers_init.hpp"
#include "os_filesystem.hpp"
#include "device.hpp"
#include "thread_group.hpp"
#include "context.hpp"
#include "environment.hpp"

using namespace Granite;
using namespace Vulkan;

static int main_inner()
{
	if (!Context::init_loader(nullptr))
		return 1;

	Context ctx;

	Context::SystemHandles handles;
	handles.filesystem = GRANITE_FILESYSTEM();
	handles.thread_group = GRANITE_THREAD_GROUP();
	ctx.set_system_handles(handles);

	if (!ctx.init_instance_and_device(nullptr, 0, nullptr, 0))
		return 1;

	Device dev;
	dev.set_context(ctx);

	auto cmd = dev.request_command_buffer();
	cmd->set_program("assets://shaders/sampler_precision.comp");

	const unsigned TEX_SIZE = 223;

	BufferCreateInfo buf;
	buf.size = TEX_SIZE * 4 * sizeof(float);
	buf.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	buf.domain = BufferDomain::CachedCoherentHostPreferCoherent;
	auto ssbo = dev.create_buffer(buf);

	auto imageinfo1 = ImageCreateInfo::immutable_2d_image(TEX_SIZE, 1, VK_FORMAT_R32_SFLOAT);
	float pixels1[TEX_SIZE];
	for (unsigned i = 0; i < TEX_SIZE; i++) {
		pixels1[i] = float(i);
	}
	ImageInitialData data1 = { pixels1 };
	auto img1 = dev.create_image(imageinfo1, &data1);

	auto imageinfo2 = ImageCreateInfo::immutable_2d_image(1, 1, VK_FORMAT_R32_SFLOAT);
	float pixels2[1] = {223.0f};
	ImageInitialData data2 = { pixels2 };
	auto img2 = dev.create_image(imageinfo2, &data2);

	cmd->set_texture(0, 0, img1->get_view(), StockSampler::NearestClamp);
	cmd->set_texture(0, 1, img2->get_view(), StockSampler::NearestClamp);
	cmd->set_storage_buffer(0, 2, *ssbo);
	cmd->dispatch(TEX_SIZE, 1, 1);
	cmd->barrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT);
	dev.submit(cmd);
	dev.wait_idle();

	auto *ptr = static_cast<const float *>(dev.map_host_buffer(*ssbo, MEMORY_ACCESS_READ_BIT));
	for (unsigned i = 0; i < TEX_SIZE; i++)
	{
		LOGI("arr[%u]: %f, x coord %.20f = 0x%.8x (texel coord %.20f = 0x%.8x)\n",
		     i,
		     ptr[i * 4 + 0],
		     ptr[i * 4 + 1], ((uint32_t *)ptr)[i * 4 + 1],
		     ptr[i * 4 + 2], ((uint32_t *)ptr)[i * 4 + 2]);
		if (i && (ptr[i * 4 + 0] - ptr[i * 4 - 4] < 0.5f)) {
			LOGI("!!!!");
		}
	}

	return 0;
}

int main()
{
	Global::init();

#ifdef ASSET_DIRECTORY
	auto asset_dir = Util::get_environment_string("ASSET_DIRECTORY", ASSET_DIRECTORY);
	GRANITE_FILESYSTEM()->register_protocol("assets", std::unique_ptr<FilesystemBackend>(new OSFilesystem(asset_dir)));
#endif
	int ret = main_inner();
	Global::deinit();
	return ret;
}
