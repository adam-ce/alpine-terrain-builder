/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 Adam Celarek <last name at cg dot tuwien dot ac dot at>
 * Copyright (C) 2022 alpinemaps.org
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include "ProgressIndicator.h"

#include <chrono>
#include <iostream>
#include <thread>
#include <cmath>

#include <fmt/core.h>

using namespace std::literals;

ProgressIndicator::ProgressIndicator(size_t n_steps)
    : m_n_steps(n_steps) {
}

void ProgressIndicator::task_finished() {
    ++m_step;
    if (m_step > m_n_steps) {
        throw std::runtime_error("Too many steps reported.");
    }
}

std::jthread ProgressIndicator::start_monitoring() const {
    const auto print = [this](size_t delta_v, std::chrono::milliseconds delta_t) {
        const auto delta_t_in_secs = std::chrono::duration_cast<std::chrono::duration<float>>(delta_t).count();
        const auto print_out = fmt::format("{}  {}, {}/s", this->progress_bar(), this->x_of_y_done_message(), float(delta_v) / delta_t_in_secs);
        std::cout << '\r' << print_out;
        std::cout.flush();
    };

    const auto t0 = std::chrono::steady_clock::now();
    std::jthread thread([=, this]() {
        const auto delta_t = 500ms;
        auto v_t_minus_1 = this->m_step.load();
        do {
            auto v_t = this->m_step.load();
            const auto delta_v = v_t - v_t_minus_1;
            v_t_minus_1 = v_t;
            print(delta_v, delta_t);
            std::this_thread::sleep_for(delta_t);
        } while (this->m_step < this->m_n_steps);
        const auto t1 = std::chrono::steady_clock::now();
        print(this->m_n_steps, std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0));
        std::cout << std::endl;
    });
    return thread;
}

std::string ProgressIndicator::progress_bar(const uint32_t bar_width) const {
    assert(bar_width >= 2);
    const auto inner_bar_width = bar_width - 2;
    const auto step = m_step.load();
    const auto progress = step > 0 ? inner_bar_width * step / m_n_steps : 0;
    assert(progress <= bar_width);

    std::ostringstream oss;
    oss << "[";
    for (uint32_t i = 0; i < inner_bar_width; i++) {
        if (i < progress || step == m_n_steps) {
            oss << "=";
        } else if (i == progress && step != 0) {
            oss << ">";
        } else {
            oss << " ";
        }
    }
    oss << "]";
    return oss.str();
}

std::string ProgressIndicator::x_of_y_done_message() const {
    return fmt::format("{}/{}", m_step.load(), m_n_steps);
}
