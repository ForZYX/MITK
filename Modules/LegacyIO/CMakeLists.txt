mitkFunctionCheckCompilerFlags("/wd4996" CMAKE_CXX_FLAGS)
mitkFunctionCheckCompilerFlags("-Wno-deprecated-declarations" CMAKE_CXX_FLAGS)

MITK_CREATE_MODULE(
  DEPENDS MitkCore
  PACKAGE_DEPENDS PRIVATE ITK|ITKIOGDCM+ITKIORAW tinyxml
  DEPRECATED_SINCE 2014.10
)
