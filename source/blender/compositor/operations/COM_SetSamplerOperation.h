/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2011, Blender Foundation.
 */

#pragma once

#include "COM_NodeOperation.h"

namespace blender::compositor {

/**
 * this program converts an input color to an output Sampler.
 * it assumes we are in sRGB color space.
 */
class SetSamplerOperation : public NodeOperation {
 private:
  PixelSampler sampler_;
  SocketReader *reader_;

 public:
  /**
   * Default constructor
   */
  SetSamplerOperation();

  void set_sampler(PixelSampler sampler)
  {
    sampler_ = sampler;
  }

  /**
   * The inner loop of this operation.
   */
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;
  void init_execution() override;
  void deinit_execution() override;
};

}  // namespace blender::compositor