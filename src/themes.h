//
// Created by Kez Cleal on 25/07/2022.
//
#pragma once

#include <GLFW/glfw3.h>
#include <iostream>
#include <pwd.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <unordered_map>

#define SK_GL

#include "include/gpu/GrBackendSurface.h"
#include "include/gpu/GrDirectContext.h"
#include "include/gpu/gl/GrGLInterface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"
#include "include/core/SkData.h"
#include "include/core/SkStream.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSize.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPoint.h"
#include "include/core/SkTypeface.h"
#include "include/core/SkFont.h"
#include "include/core/SkTextBlob.h"

#include "../inc/robin_hood.h"
#include "argparse.h"

#include "glob.h"
#include "ini.h"
#include "utils.h"


namespace Themes {

    constexpr float base_qual_alpha[11] = {51, 51, 51, 51, 51, 128, 128, 128, 128, 128, 255};

    class BaseTheme {
    public:
        BaseTheme();
        ~BaseTheme() = default;

        std::string name;
        // face colours
        SkPaint bgPaint, fcNormal, fcDel, fcDup, fcInvF, fcInvR, fcTra, fcIns, fcSoftClip, \
                fcA, fcT, fcC, fcG, fcN, fcCoverage;
        SkPaint fcNormal0, fcDel0, fcDup0, fcInvF0, fcInvR0, fcTra0, fcSoftClip0;

        std::vector<SkPaint> mate_fc;
        std::vector<SkPaint> mate_fc0;

        // edge colours
        SkPaint ecMateUnmapped, ecSplit;

        // line widths
        float lwMateUnmapped, lwSplit, lwCoverage;

        // line colours and Insertion pain
        SkPaint lcJoins, lcCoverage, insF, insS;

        // text colours
        SkPaint tcDel, tcIns, tcLabels;

        // Markers
        SkPaint marker_paint;

        uint8_t alpha, mapq0_alpha;

        std::vector<SkPaint> APaint, TPaint, CPaint, GPaint, NPaint;

        void setAlphas();

    };

    class IgvTheme: public BaseTheme {
        public:
            IgvTheme();
            ~IgvTheme() = default;
    };

    class DarkTheme: public BaseTheme {
        public:
            DarkTheme();
            ~DarkTheme() = default;
    };


    class IniOptions {
    public:
        IniOptions();
        ~IniOptions() = default;

        BaseTheme theme;
        Utils::Dims dimensions, number;
        std::string fmt, labels, link;
        int indel_length, ylim, split_view_size, threads, pad, link_op;
        bool no_show, log2_cov, tlen_yscale;
        float scroll_speed, tab_track_height;
        int scroll_right;
        int scroll_left;
        int scroll_down;
        int scroll_up;
        int next_region_view;
        int zoom_out;
        int zoom_in;
        int cycle_link_mode;
        int print_screen;
        int delete_labels;
        int enter_interactive_mode;

        robin_hood::unordered_map<std::string, std::string> references;
        robin_hood::unordered_map<std::string, std::vector<std::string>> tracks;

        void readIni(std::string path);
    };

}