#include <vtkSmartPointer.h>

#include <vtkDicomReader.h>
#include <vtkDICOMSorter.h>
#include <vtkStringArray.h>
#include <vtkNIFTIImageReader.h>
#include <vtkImageData.h>

#include <vtkAxesActor.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>

#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkInteractorStyleImage.h>
#include <vtkInteractorStyleTrackballCamera.h>

#include <vtkImageActor.h>
#include <vtkImageViewer2.h>
#include <vtkImageSliceMapper.h>
#include <vtkLookupTable.h>
#include <vtkImageProperty.h>
#include <vtkImageMapToColors.h>

#include <vtkCommand.h>
#include <vtkWidgetEvent.h>
#include <vtkCallbackCommand.h>
#include <vtkWidgetEventTranslator.h>

#include <vtkSliderWidget.h>
#include <vtkSliderRepresentation2D.h>
#include <vtkProperty.h>
#include <vtkTextProperty.h>
#include <vtkProperty2D.h>

#include <vtkCornerAnnotation.h>

#include <filesystem>
#include <iostream>
#include <sstream>

//#define USE_SLIDER

class MImageViewer2: public vtkImageViewer2
{
public:
    vtkTypeMacro(MImageViewer2, vtkImageViewer2);

    static MImageViewer2* New() { return new MImageViewer2; }

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

#ifdef USE_SLIDER
class MSliderCallback: public vtkCommand
{
public:
    static MSliderCallback* New() { return new MSliderCallback; }
    virtual void Execute(vtkObject* caller, unsigned long, void*)
    {
        vtkSliderWidget* sliderWidget = reinterpret_cast<vtkSliderWidget*>(caller);

        this->viewer->SetSlice(static_cast<vtkSliderRepresentation*>(sliderWidget->GetRepresentation())->GetValue());
        if (this->viewer1 != nullptr)
            this->viewer1->SetSlice(
                static_cast<vtkSliderRepresentation*>(sliderWidget->GetRepresentation())->GetValue());
        this->viewer->Render();
    }
    MSliderCallback() {}
    vtkSmartPointer<MImageViewer2> viewer = nullptr;
    vtkSmartPointer<MImageViewer2> viewer1 = nullptr;
};
#else
class myInteractorStyler final: public vtkInteractorStyleImage
{
public:
    static myInteractorStyler* New();

    vtkTypeMacro(myInteractorStyler, vtkInteractorStyleImage);

    void setImageViewers(vtkImageViewer2* viewer1, vtkImageViewer2* viewer2)
    {
        m_viewer1 = viewer1;
        m_viewer2 = viewer2;
        m_slice_min = viewer1->GetSliceMin();
        m_slice_max = viewer1->GetSliceMax();
        m_slice = (m_slice_min + m_slice_max) / 2;
    }

protected:
    void OnMouseWheelForward() override { moveSliceForward(); }

    void OnMouseWheelBackward() override { moveSliceBackward(); }

private:
    void moveSliceForward()
    {
        if (m_slice < m_slice_max)
        {
            m_slice += 1;

            m_viewer1->SetSlice(m_slice);
            m_viewer2->SetSlice(m_slice);
            m_viewer1->Render();
        }
        std::cout << m_slice << '\n';
    }

    void moveSliceBackward()
    {
        if (m_slice > m_slice_min)
        {
            m_slice -= 1;

            m_viewer1->SetSlice(m_slice);
            m_viewer2->SetSlice(m_slice);
            m_viewer1->Render();
        }
        std::cout << m_slice << '\n';
    }

private:
    vtkImageViewer2 *m_viewer1, *m_viewer2;
    int m_slice;
    int m_slice_min;
    int m_slice_max;
};
vtkStandardNewMacro(myInteractorStyler);
#endif

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

void overlay(vtkSmartPointer<vtkImageData> dicom, vtkSmartPointer<vtkImageData> nii)
{
    vtkSmartPointer<MImageViewer2> viewer = vtkSmartPointer<MImageViewer2>::New();
    viewer->SetInputData(dicom);
    viewer->SetSlice(255);
    viewer->SetSliceOrientationToXY();
    viewer->GetRenderWindow()->SetWindowName("overlay");
    viewer->GetRenderWindow()->SetSize(500, 500);

    vtkSmartPointer<vtkLookupTable> pColorTable = vtkSmartPointer<vtkLookupTable>::New();
    pColorTable->SetNumberOfColors(2);
    pColorTable->SetTableRange(nii->GetScalarRange());
    pColorTable->SetTableValue(0, 0.0, 0.0, 1.0, 0.0);
    pColorTable->SetTableValue(1, 1, 0, 0, 1.0);
    pColorTable->Build();
    vtkSmartPointer<MImageViewer2> viewerLayer = vtkSmartPointer<MImageViewer2>::New();
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

#ifdef USE_SLIDER
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

    vtkSmartPointer<MSliderCallback> callback = vtkSmartPointer<MSliderCallback>::New();
    callback->viewer = viewer;
    callback->viewer1 = viewerLayer;

    sliderWidget->AddObserver(vtkCommand::InteractionEvent, callback);
#else
    vtkNew<myInteractorStyler> style;
    style->setImageViewers(viewer, viewerLayer);
    rwi->SetInteractorStyle(style);
#endif

    // fps
    vtkNew<vtkCornerAnnotation> corner_overlay;
    corner_overlay->GetTextProperty()->SetColor(1.0, 0.72, 0.0);
    corner_overlay->SetText(vtkCornerAnnotation::UpperRight, "FPS: 0");
    vtkNew<vtkCallbackCommand> fps_callback;
    fps_callback->SetCallback(
        [](vtkObject* caller, long unsigned int vtkNotUsed(eventId), void* clientData, void* vtkNotUsed(callData)) {
            auto* renderer = static_cast<vtkRenderer*>(caller);
            auto* corner_overlay = reinterpret_cast<vtkCornerAnnotation*>(clientData);
            double timeInSeconds = renderer->GetLastRenderTimeInSeconds();
            double fps = 1.0 / timeInSeconds;
            std::ostringstream out;
            out << "FPS: ";
            out.precision(2);
            out << fps;
            corner_overlay->SetText(vtkCornerAnnotation::UpperRight, out.str().c_str());
        });
    fps_callback->SetClientData(corner_overlay.Get());
    viewer->GetRenderer()->AddViewProp(corner_overlay);
    viewer->GetRenderer()->AddObserver(vtkCommand::EndEvent, fps_callback);

    rwi->Start();
}

int main(int argc, char* argv[])
{
    const char* dicom_path = argv[1];
    const char* nii_path = argv[2];
    vtkSmartPointer<vtkImageData> dicom = LoadDicom(dicom_path);
    vtkSmartPointer<vtkImageData> nii = LoadNii(nii_path);
    overlay(dicom, nii);

    return 0;
}
