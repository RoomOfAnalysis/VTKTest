#include <vtkSmartPointer.h>
#include <vtkNew.h>
#include <vtkDICOMReader.h>
#include <vtkDICOMSorter.h>
#include <vtkStringArray.h>
#include <vtkImageData.h>

#include <filesystem>

namespace
{
    vtkSmartPointer<vtkImageData> ReadDicomFolder(const char* dirpath)
    {
        std::filesystem::path dir_path{dirpath};
        if (!std::filesystem::is_directory(dir_path)) return {};

        vtkNew<vtkDICOMReader> dicom_reader;
        vtkNew<vtkStringArray> dicom_img_paths;
        vtkNew<vtkDICOMSorter> sorter;
        int count = 0;
        for (auto const& it : std::filesystem::directory_iterator(dir_path))
            dicom_img_paths->InsertValue(count++, it.path().string());
        sorter->SetInputFileNames(dicom_img_paths);
        sorter->Update();
        //dicom_reader->SetMemoryRowOrderToFileNative();
        dicom_reader->SetFileNames(sorter->GetFileNamesForSeries(0));
        dicom_reader->SetDataByteOrderToLittleEndian();
        dicom_reader->Update(0);
        return dicom_reader->GetOutput();
    }
} // namespace