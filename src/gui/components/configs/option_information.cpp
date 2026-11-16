#include "common/i18n.h"
#include "configs.h"

#include "../../ui/ui.h"
#include "../../render/render.h"

namespace configs = gui::components::configs;

void configs::option_information(ui::Container& container) {
	const static std::unordered_map<std::string, std::vector<std::string>> option_explanations = {
		// Blur settings
		// { "section blur checkbox",
		//   {
		// 	  "启用运动模糊",
		//   }, },
		{
			"blur amount",
			{
				i18n::t("运动模糊强度"),
				i18n::t("(0 = 无模糊，1 = 完全混合所有帧，>1 = 混合额外帧（残影）)"),
			},
		},
		// { "output fps",
		//   {
		// 	  "输出视频的帧率",
		//   }, },
		{
			"blur gamma",
			{
				i18n::t("模糊前将画面压暗的程度，可让高光更突出"),
			},
		},
		{
			"blur weighting gaussian std dev slider",
			{
				i18n::t("高斯模糊权重的标准差"),
			},
		},
		{
			"blur weighting triangle reverse checkbox",
			{
				i18n::t("反转三角权重的方向"),
			},
		},
		{
			"blur weighting bound input",
			{
				i18n::t("权重边界，用于让权重分布更均匀"),
			},
		},

		// Interpolation settings
		// { "section interpolation checkbox",
		//   {
		// 	  "在模糊前插帧到更高帧率",
		//   }, },
		{
			"interpolate scale checkbox",
			{
				i18n::t("使用倍数而不是固定帧率来插帧"),
			},
		},
		{
			"interpolated fps mult",
			{
				i18n::t("插帧倍数"),
				i18n::t("输入视频将被插帧到此帧率（模糊前）"),
			},
		},
		{
			"interpolated fps",
			{
				i18n::t("输入视频插帧的目标帧率（模糊前）"),
			},
		},
		{
			"interpolation method dropdown",
			{
				i18n::t("质量：rife > svp"),
				i18n::t("速度：svp > rife"),
			},
		},
		// pre-interp settings
		{
			"section pre-interpolation checkbox",
			{
				i18n::t("在主插帧前，使用更精确但更慢的 AI 模型进行预插帧"),
			},
		},
		{
			"pre-interpolated fps mult",
			{
				i18n::t("预插帧倍数"),
				i18n::t("输入视频将被插帧到此帧率（主插帧与模糊之前）"),
			},
		},
		{
			"pre-interpolated fps",
			{
				i18n::t("输入视频预插帧的目标帧率（模糊前）"),
			},
		},
		{
			"SVP interpolation preset dropdown",
			{
				i18n::t("更多信息请查看 blur 的 GitHub 页面"),
			},
		},
		{
			"SVP interpolation algorithm dropdown",
			{
				i18n::t("更多信息请查看 blur 的 GitHub 页面"),
			},
		},
		{
			"interpolation block size dropdown",
			{
				i18n::t("插帧的块大小"),
				i18n::t("（越大越粗糙但更快，越小越精细但更慢）"),
			},
		},
		{
			"interpolation mask area slider",
			{
				i18n::t("插帧的遮罩量"),
				i18n::t("（越大越能减少静止物体的模糊，但可能影响流畅度）"),
			},
		},

		// Rendering settings
		{
			"deduplicate checkbox",
			{
				i18n::t("去除重复帧，并用插帧结果替换"),
				i18n::t("（修复录制卡顿导致的输出不顺畅）"),
			},
		},
		{
			"deduplicate range",
			{
				i18n::t("去重时向后查找唯一帧的帧数范围"),
				i18n::t("如果素材帧率偏低（例如 120fps 的游戏画面以 240fps 录制），请调大 "),
				i18n::t("如果静态元素（例如菜单界面）也被模糊，请调小"),
			},
		},
		{
			"deduplicate threshold input",
			{
				i18n::t("触发去重的运动阈值"),
				i18n::t("在“高级”中开启调试并渲染视频，即可在画面中看到每帧的运动量"),
			},
		},
		{
			"deduplicate method dropdown",
			{
				i18n::t("质量：rife > svp"),
				i18n::t("速度：old > svp > rife"),
			},
		},
		{
			"preview checkbox",
			{
				i18n::t("渲染时显示预览"),
			},
		},
		{
			"detailed filenames checkbox",
			{
				i18n::t("在生成的文件名中加入模糊设置"),
			},
		},

		// Timescale settings
		// {
		// 	"section timescale checkbox",
		// 	{
		// 		"启用视频时间尺度调整",
		// 	},
		// },
		// {
		// 	"input timescale",
		// 	{
		// 		"输入视频的时间尺度",
		// 	},
		// },
		// {
		// 	"output timescale",
		// 	{
		// 		"输出视频的时间尺度",
		// 	},
		// },
		{
			"adjust timescaled audio pitch checkbox",
			{
				i18n::t("变速时同步改变音频音调"),
			},
		},

		// Filters
		// { "section filters checkbox", { "启用视频滤镜", }, },
		// { "brightness", { "调整输出视频的亮度", }, },
		// { "saturation", { "调整输出视频的饱和度", }, },
		// { "contrast", { "调整输出视频的对比度", }, },

		// Advanced rendering
		// { "gpu interpolation checkbox", { "使用 GPU 进行插帧", }, },
		// { "gpu encoding checkbox", { "使用 GPU 进行渲染", }, },
		// { "gpu encoding type dropdown", { "选择 GPU 类型", }, },
		{
			"video container text input",
			{
				i18n::t("输出视频的容器格式"),
			},
		},
		{
			"custom ffmpeg filters text input",
			{
				i18n::t("渲染时使用的自定义 FFmpeg 滤镜"),
				i18n::t("（会覆盖 GPU 与质量选项）"),
			},
		},
		// { "debug checkbox", { "显示调试窗口，并打印 blur 使用的命令", } }
		{
			"copy dates checkbox",
			{
				i18n::t("复制输入文件的修改日期"),
			},
		},
	};

	std::string hovered = ui::get_hovered_id();

	if (hovered.empty())
		return;

	if (!option_explanations.contains(hovered))
		return;

	ui::add_hint("hovered option info", container, option_explanations.at(hovered), gfx::Color::white(), fonts::dejavu);
}
