#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkOMETIFFReader.h>
#include <vtkImageViewer2.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleImage.h>
#include <vtkRenderWindow.h>

#include <vtkCornerAnnotation.h>
#include <vtkTextProperty.h>
#include <vtkCallbackCommand.h>
#include <vtkRenderer.h>
#include <vtkObjectFactory.h>
#include <vtkImageHistogramStatistics.h>
#include <vtkImageMapToWindowLevelColors.h>

#include <vtkInformation.h>

#include <filesystem>
#include <sstream>
#include <iostream>

class myInteractorStyler final: public vtkInteractorStyleImage
{
public:
    static myInteractorStyler* New();

    vtkTypeMacro(myInteractorStyler, vtkInteractorStyleImage);

    void setImageViewer(vtkImageViewer2* imageViewer, int slice_no = 0)
    {
        m_viewer = imageViewer;
        m_slice_min = imageViewer->GetSliceMin();
        m_slice_max = imageViewer->GetSliceMax();
        m_slice = slice_no <= 0 ? (m_slice_min + m_slice_max) / 2 : slice_no;
        m_viewer->SetSlice(m_slice);

        if (!m_text)
        {
            m_text = vtkSmartPointer<vtkCornerAnnotation>::New();
            m_text->GetTextProperty()->SetColor(1.0, 0.72, 0.0);
            m_viewer->GetRenderer()->AddViewProp(m_text);
        }
        ShowSliceText();
    }

protected:
    void OnMouseWheelForward() override { moveSliceForward(); }

    void OnMouseWheelBackward() override { moveSliceBackward(); }

    void OnChar() override
    {
        if (Interactor->GetKeyCode() == 'x')
            m_viewer->SetSliceOrientationToYZ();
        else if (Interactor->GetKeyCode() == 'y')
            m_viewer->SetSliceOrientationToXZ();
        else if (Interactor->GetKeyCode() == 'z')
            m_viewer->SetSliceOrientationToXY();
    }

private:
    void moveSliceForward()
    {
        if (m_slice < m_slice_max)
        {
            m_slice += 1;

            m_viewer->SetSlice(m_slice);
            ShowSliceText();
        }
    }

    void moveSliceBackward()
    {
        if (m_slice > m_slice_min)
        {
            m_slice -= 1;

            m_viewer->SetSlice(m_slice);
            ShowSliceText();
        }
    }

    void ShowSliceText()
    {
        std::stringstream ss;
        ss << m_slice << " / " << m_slice_max;
        m_text->SetText(vtkCornerAnnotation::LowerRight, ss.str().c_str());
    }

private:
    vtkImageViewer2* m_viewer;
    int m_slice;
    int m_slice_min;
    int m_slice_max;
    vtkSmartPointer<vtkCornerAnnotation> m_text = nullptr;
};
vtkStandardNewMacro(myInteractorStyler);

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "1) .ome.tiff file path" << std::endl;
        return EXIT_FAILURE;
    }

    std::filesystem::path path{argv[1]};

    vtkNew<vtkOMETIFFReader> reader;
    if (!reader->CanReadFile(path.string().c_str()))
    {
        std::cerr << "ERROR: vtkOMETIFFReader cannot read the provided file: " << path << std::endl;
        return EXIT_FAILURE;
    }
    reader->SetFileName(path.string().c_str());
    reader->Update();
    auto img_data = reader->GetOutput();

    std::cout << img_data->GetScalarTypeAsString() << std::endl;

    //img_data->PrintSelf(std::cout, vtkIndent{});
    // according to https://github.com/Kitware/VTK/blob/master/IO/Image/vtkOMETIFFReader.cxx
    // vtk only reads the following XML fields...
    //omeinternals.IsValid = true;
    //omeinternals.SizeX = pixelsXML.attribute("SizeX").as_int(0);
    //omeinternals.SizeY = pixelsXML.attribute("SizeY").as_int(0);
    //omeinternals.SizeZ = pixelsXML.attribute("SizeZ").as_int(1);
    //omeinternals.SizeC = pixelsXML.attribute("SizeC").as_int(1);
    //omeinternals.SizeT = pixelsXML.attribute("SizeT").as_int(1);
    //omeinternals.TimeIncrement = pixelsXML.attribute("TimeIncrement").as_double(1.0);
    //omeinternals.PhysicalSize[0] = pixelsXML.attribute("PhysicalSizeX").as_double(1.0);
    //omeinternals.PhysicalSize[1] = pixelsXML.attribute("PhysicalSizeY").as_double(1.0);
    //omeinternals.PhysicalSize[2] = pixelsXML.attribute("PhysicalSizeZ").as_double(1.0);
    //omeinternals.PhysicalSizeUnit[0] = pixelsXML.attribute("PhysicalSizeXUnit").as_string();
    //omeinternals.PhysicalSizeUnit[1] = pixelsXML.attribute("PhysicalSizeYUnit").as_string();
    //omeinternals.PhysicalSizeUnit[2] = pixelsXML.attribute("PhysicalSizeZUnit").as_string();

    img_data->GetInformation()->Print(std::cout);

    vtkNew<vtkImageViewer2> viewer;
    viewer->SetInputData(img_data);
    viewer->SetSliceOrientationToXY();

    vtkNew<vtkRenderWindowInteractor> interactor;
    vtkNew<myInteractorStyler> style;
    style->setImageViewer(viewer);
    viewer->SetupInteractor(interactor); // this line should be put before the next line... otherwise the style not work
    interactor->SetInteractorStyle(style);
    viewer->GetRenderWindow()->SetWindowName("ome tiff viewer");
    viewer->GetRenderWindow()->SetSize(500, 500);
    viewer->Render();
    interactor->Start();

    return 0;
}