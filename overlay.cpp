#pragma once

#include <vtkAutoInit.h>
VTK_MODULE_INIT(vtkRenderingOpenGL2);
VTK_MODULE_INIT(vtkRenderingVolumeOpenGL2);
VTK_MODULE_INIT(vtkInteractionStyle);
VTK_MODULE_INIT(vtkRenderingFreeType);
#include <vtkImageData.h>
#include <vtkProperty.h>
#include <vtkDataSetMapper.h>
#include <vtkRendererCollection.h>
#include <vtkPolyDataMapper.h>
#include <vtkDicomReader.h>
#include <vtkDICOMSorter.h>
#include <vtkStringArray.h>
#include <vtkNIFTIImageReader.h>

#include <vtkAxesActor.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>

#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkInteractorStyleImage.h>

#include <vtkSmartPointer.h>
#include <vtkImageActor.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkImageViewer2.h>
#include <vtkImageSliceMapper.h>
#include <vtkPNGReader.h>
#include <vtkSTLReader.h>
#include <vtkTexture.h>
#include <vtkTextureMapToCylinder.h>
#include <vtkImageFlip.h>
#include <vtkImageGradient.h>
#include <vtkImageGradient.h>
#include <vtkImageMagnitude.h>
#include <vtkImageShiftScale.h>
#include <vtkImageHybridMedian2D.h>
#include <vtkSphereSource.h>
#include <vtkTextProperty.h>
#include <vtkProperty2D.h>
#include <vtkSmartPointer.h>
#include <vtkPolyData.h>
#include <vtkSliderWidget.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSmartPointer.h>
#include <vtkCommand.h>
#include <vtkWidgetEvent.h>
#include <vtkCallbackCommand.h>
#include <vtkWidgetEventTranslator.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkSliderWidget.h>
#include <vtkSliderRepresentation2D.h>
#include <vtkProperty.h>
#include <vtkLookupTable.h>
#include <vtkImageProperty.h>
#include <vtkImageMapToColors.h>

#include <filesystem>
#include <iostream>

class vtkImageViewer2My : public vtkImageViewer2
{
public:
    vtkTypeMacro(vtkImageViewer2My, vtkImageViewer2);

    static vtkImageViewer2My* New() { return new vtkImageViewer2My; }

    void SetSlice(int slice) override
    {
        int* range = this->GetSliceRange();
        if (range)
        {
            if (slice < range[0])
                slice = range[0];
            else if (slice > range[1])
                slice = range[1];
        }

        if (this->Slice == slice) return;

        this->Slice = slice;
        this->Modified();

        this->UpdateDisplayExtent();
        //this->Render();
    }
};

class vtkSliderCallback1: public vtkCommand
{
public:
    static vtkSliderCallback1* New() { return new vtkSliderCallback1; }
    virtual void Execute(vtkObject* caller, unsigned long, void*)
    {
        vtkSliderWidget* sliderWidget = reinterpret_cast<vtkSliderWidget*>(caller);

        this->viewer->SetSlice(static_cast<vtkSliderRepresentation*>(sliderWidget->GetRepresentation())->GetValue());
        if (this->viewer1 != nullptr)
            this->viewer1->SetSlice(
                static_cast<vtkSliderRepresentation*>(sliderWidget->GetRepresentation())->GetValue());
        this->viewer->Render();
    }
    vtkSliderCallback1() {}
    vtkSmartPointer<vtkImageViewer2My> viewer = nullptr;
    vtkSmartPointer<vtkImageViewer2My> viewer1 = nullptr;
};

vtkSmartPointer<vtkImageData> LoadDicom(const char* path)
{
    try
    {
        std::filesystem::path dicom_dir_path{path};
        vtkNew<vtkDICOMReader> reader;
        vtkNew<vtkStringArray> dicom_img_paths;
        vtkNew<vtkDICOMSorter> dicom_sorter;
        int count = 0;
        for (auto const& it : std::filesystem::directory_iterator(dicom_dir_path))
            dicom_img_paths->InsertValue(count++, it.path().string());
        dicom_sorter->SetInputFileNames(dicom_img_paths);
        dicom_sorter->Update();
        reader->SetFileNames(dicom_sorter->GetFileNamesForSeries(0));
        reader->SetDataByteOrderToLittleEndian();
        reader->Update(0);
        return reader->GetOutput();
    }
    catch (std::exception const& e)
    {
        std::cerr << e.what() << std::endl;
    }
    return nullptr;
}

vtkSmartPointer<vtkImageData> LoadNii(const char* file_path)
{
    vtkNew<vtkNIFTIImageReader> nii_reader;
    if (!nii_reader->CanReadFile(file_path))
    {
        std::cerr << "vtk NIFTI image reader cannot read the provided file: " << file_path << std::endl;
        return nullptr;
    }
    nii_reader->SetFileName(file_path);
    nii_reader->Update();
    return nii_reader->GetOutput();
}

void show3dImage(vtkSmartPointer<vtkImageData> dicom, vtkSmartPointer<vtkImageData> nii)
{
    vtkSmartPointer<vtkImageViewer2My> viewer = vtkSmartPointer<vtkImageViewer2My>::New();
    viewer->SetInputData(dicom);
    viewer->SetSlice(255);
    viewer->SetSliceOrientationToXY();
    viewer->GetRenderer()->SetBackground(0.5, 0.5, 0.5);
    viewer->GetRenderWindow()->SetWindowName("ImageViewer2D");

    vtkSmartPointer<vtkLookupTable> pColorTable = vtkSmartPointer<vtkLookupTable>::New();
    pColorTable->SetNumberOfColors(2);
    pColorTable->SetTableRange(nii->GetScalarRange());
    pColorTable->SetTableValue(0, 0.0, 0.0, 1.0, 0.0);
    pColorTable->SetTableValue(1, 1, 0, 0, 1.0);
    pColorTable->Build();
    vtkSmartPointer<vtkImageViewer2My> viewerLayer = vtkSmartPointer<vtkImageViewer2My>::New();
    viewerLayer->SetInputData(nii);
    viewerLayer->SetRenderWindow(viewer->GetRenderWindow());
    viewerLayer->SetSliceOrientationToXY();
    viewerLayer->SetSlice(255);
    viewerLayer->GetImageActor()->SetInterpolate(false);
    viewerLayer->GetImageActor()->GetProperty()->SetLookupTable(pColorTable);
    viewerLayer->GetImageActor()->GetProperty()->SetDiffuse(0.0);
    viewerLayer->GetImageActor()->SetPickable(false);

    viewer->GetRenderer()->AddActor(viewerLayer->GetImageActor());

    vtkSmartPointer<vtkRenderWindowInteractor> rwi = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    viewer->SetupInteractor(rwi);
    //vtkSmartPointer<vtkInteractorStyleTrackballCamera> style =
    //    vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
    ////viewer->GetRenderer()->GetRenderWindow()->GetInteractor()->SetInteractorStyle(style);

    vtkSmartPointer<vtkSliderRepresentation2D> sliderRep = vtkSmartPointer<vtkSliderRepresentation2D>::New();
    sliderRep->SetMinimumValue(viewer->GetSliceMin());
    sliderRep->SetMaximumValue(viewer->GetSliceMax());
    sliderRep->SetValue(5.0);
    sliderRep->GetSliderProperty()->SetColor(1, 0, 0);   //red
    sliderRep->GetTitleProperty()->SetColor(1, 0, 0);    //red
    sliderRep->GetLabelProperty()->SetColor(1, 0, 0);    //red
    sliderRep->GetSelectedProperty()->SetColor(0, 1, 0); //green
    sliderRep->GetTubeProperty()->SetColor(1, 1, 0);     //yellow
    sliderRep->GetCapProperty()->SetColor(1, 1, 0);      //yellow
    sliderRep->GetPoint1Coordinate()->SetCoordinateSystemToDisplay();
    sliderRep->GetPoint1Coordinate()->SetValue(40, 40);
    sliderRep->GetPoint2Coordinate()->SetCoordinateSystemToDisplay();
    sliderRep->GetPoint2Coordinate()->SetValue(500, 40);
    vtkSmartPointer<vtkSliderWidget> sliderWidget = vtkSmartPointer<vtkSliderWidget>::New();
    sliderWidget->SetInteractor(rwi);
    sliderWidget->SetRepresentation(sliderRep);
    sliderWidget->SetAnimationModeToAnimate();
    sliderWidget->EnabledOn();

    vtkSmartPointer<vtkSliderCallback1> callback = vtkSmartPointer<vtkSliderCallback1>::New();
    callback->viewer = viewer;
    callback->viewer1 = viewerLayer;

    sliderWidget->AddObserver(vtkCommand::InteractionEvent, callback);

    //vtkSmartPointer<vtkAxesActor> axes = vtkSmartPointer<vtkAxesActor>::New();
    //double axesSize[3] = {100, 100, 100};
    //axes->SetTotalLength(axesSize);
    //axes->SetConeRadius(0.1);
    //axes->SetShaftTypeToLine();
    //axes->SetAxisLabels(false);
    //viewer->GetRenderer()->AddActor(axes);

    rwi->Start();
}

int main(int argc, char* argv[])
{
    const char* dicom_path = argv[1];
    const char* nii_path = argv[2];
    vtkSmartPointer<vtkImageData> dicom = LoadDicom(dicom_path);
    vtkSmartPointer<vtkImageData> nii = LoadNii(nii_path);
    show3dImage(dicom, nii);

    return 0;
}
