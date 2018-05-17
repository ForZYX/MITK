/*===================================================================

  The Medical Imaging Interaction Toolkit (MITK)

  Copyright (c) German Cancer Research Center,
  Division of Medical and Biological Informatics.
  All rights reserved.

  This software is distributed WITHOUT ANY WARRANTY; without
  even the implied warranty of MERCHANTABILITY or FITNESS FOR
  A PARTICULAR PURPOSE.

  See LICENSE.txt or http://www.mitk.org for details.

  ===================================================================*/

#include "./mitkPhotoacousticMotionCorrectionFilter.h"
#include <mitkImageReadAccessor.h>

mitk::PhotoacousticMotionCorrectionFilter::
    PhotoacousticMotionCorrectionFilter() {
  // Set the defaults for the OpticalFlowFarneback algorithm
  // The values are taken directly out of Thomas's
  // US-CV-Based-Optical-Flow-Carotis.ipyn
  m_batch = 5;
  m_pyr_scale = 0.5;
  m_levels = 1;
  m_winsize = 40;
  m_iterations = 2;
  m_poly_n = 7;
  m_poly_sigma = 1.5;
  m_flags = 0;

  this->SetNumberOfIndexedInputs(2);
  this->SetNumberOfIndexedOutputs(2);
  this->SetNthOutput(0, mitk::Image::New());
  this->SetNthOutput(1, mitk::Image::New());
}

mitk::PhotoacousticMotionCorrectionFilter::
    ~PhotoacousticMotionCorrectionFilter() {}

// TODO: Find out how to throw the right errors
void mitk::PhotoacousticMotionCorrectionFilter::CheckInput(mitk::Image::Pointer paImage, mitk::Image::Pointer usImage) {
  // Check that we actually got some images
  if (!paImage || !usImage) {
    // TODO: Throw some error here
    MITK_INFO << "We did not get two images!";
    throw std::invalid_argument("One of the images was NULL.");
  }

  // Check that the image dimensions are the same
  if (paImage->GetDimension() != 3 || usImage->GetDimension() != 3) {
    MITK_INFO << "Mismatching image dimensions detected in the motion "
      "compensation filter.";
    // TODO: Throw some error here
    throw std::invalid_argument("Both images must have dimension 3.");
  }

  // Check that each dimension has the same size
  for (unsigned int i = 0; i < paImage->GetDimension(); i++) {
    if (paImage->GetDimensions()[i] != usImage->GetDimensions()[i]) {
      MITK_INFO << "Mismatching image dimensions detected in the motion "
        "compensation filter.";
      // TODO: Throw some error here
      throw std::invalid_argument("Both images must have the same length in each dimension.");
    }
  }

}

void mitk::PhotoacousticMotionCorrectionFilter::InitializeOutput(mitk::Image::Pointer paInput, mitk::Image::Pointer usInput, mitk::Image::Pointer paOutput, mitk::Image::Pointer usOutput) {
  if (paOutput->GetDimension() != 3) {
    MITK_INFO << "I jump in here.";
    this->SetOutputData(paInput, paOutput);
    this->SetOutputData(usInput, usOutput);
  }

  for (unsigned int i = 0; i < usOutput->GetDimension(); i++) {
    if (usOutput->GetDimensions()[i] != usInput->GetDimensions()[i]) {
      this->SetOutputData(paInput, paOutput);
      this->SetOutputData(usInput, usOutput);
      break;
   }
  }
}

void mitk::PhotoacousticMotionCorrectionFilter::SetOutputData(mitk::Image::Pointer input, mitk::Image::Pointer output) {
  output->Initialize(input);
  mitk::ImageReadAccessor accessor(input);
  output->SetImportVolume(accessor.GetData());
}

void mitk::PhotoacousticMotionCorrectionFilter::PerformCorrection(mitk::Image::Pointer paInput, mitk::Image::Pointer usInput, mitk::Image::Pointer paOutput, mitk::Image::Pointer usOutput) {

  for(unsigned int i = 0; i < paInput->GetDimensions()[2]; i++) {

    // Get the 2d matrix from slice at i
    m_PaMatC = this->GetMatrix(paInput, i);
    m_UsMatC = this->GetMatrix(usInput, i);

    // Transform to cv::UMat
    m_PaMat = m_PaMatC.getUMat( cv::ACCESS_READ ).clone();
    m_UsMat = m_UsMatC.getUMat( cv::ACCESS_READ ).clone();

    // If batch size was set to 0, use one single batch for the whole data set.
    unsigned int batch;
    if (m_batch == 0) {
      batch = paInput->GetDimensions()[2];
    } else {
      batch = m_batch;
    }

    // At the beginning of a batch we set the new reference image and directly write the result images
    
    if (i % batch == 0) {
      m_UsRef = m_UsMat.clone();
      m_UsRes = m_UsMatC.clone();
      m_PaRes = m_PaMatC.clone();
    } else {
      cv::calcOpticalFlowFarneback(m_UsRef, m_UsMat, m_Flow, m_pyr_scale, m_levels,
                                   m_winsize, m_iterations, m_poly_n,
                                   m_poly_sigma, 0);

      // Apply the flow to the matrices
      cv::remap(m_PaMatC, m_PaRes, m_Flow, cv::noArray(), cv::INTER_LINEAR);
      cv::remap(m_UsMatC, m_UsRes, m_Flow, cv::noArray(), cv::INTER_LINEAR);
    }

    // Enter the matrix as a slice at position i into output
    this->EnterMatrixInPosition(m_PaRes, paOutput, i);
    this->EnterMatrixInPosition(m_UsRes, usOutput, i);
  }
}

cv::Mat mitk::PhotoacousticMotionCorrectionFilter::GetMatrix(const mitk::Image::Pointer input, unsigned int i) {

  // Access slice i of image input
  mitk::ImageReadAccessor accessor(input, input->GetSliceData(i));
  mitk::Image::Pointer slice = mitk::Image::New();
  slice->Initialize(input->GetPixelType(), 2, input->GetDimensions());
  slice->SetVolume(accessor.GetData());

  // Transform the slice to matrix
  m_ImageToOpenCVFilter->SetImage(slice);
  return m_ImageToOpenCVFilter->GetOpenCVMat();
}

void mitk::PhotoacousticMotionCorrectionFilter::EnterMatrixInPosition(cv::Mat mat, mitk::Image::Pointer output, unsigned int i) {

  m_OpenCVToImageFilter->SetOpenCVMat(mat);
  m_OpenCVToImageFilter->Update();
  mitk::Image::Pointer slice = m_OpenCVToImageFilter->GetOutput();

  mitk::ImageReadAccessor accessor(slice);
  output->SetSlice(accessor.GetData(), i);
}

// TODO: remove debug messages
void mitk::PhotoacousticMotionCorrectionFilter::GenerateData() {
  MITK_INFO << "Start motion compensation.";

  mitk::Image::Pointer paInput = this->GetInput(0);
  mitk::Image::Pointer usInput = this->GetInput(1);
  mitk::Image::Pointer paOutput = this->GetOutput(0);
  mitk::Image::Pointer usOutput = this->GetOutput(1);

  // Check that we have two input images with agreeing dimensions
  try {
    this->CheckInput(paInput, usInput);
  }
  catch(std::invalid_argument& e) {
    throw e;
  }

  // Check the output images and (re-)initialize, if necessary.
  this->InitializeOutput(paInput, usInput, paOutput, usOutput);

  // m_ImageToOpenCVFilter->SetImage(paInput);
  this->PerformCorrection(paInput, usInput, paOutput, usOutput);

  // MITK_INFO << "Input: " << usInput;
  // MITK_INFO << "Output: " << usOutput;
  // MITK_INFO << "Output2: " << paOutput;
  MITK_INFO << "We succeeded in running through the whole thing!";
}
