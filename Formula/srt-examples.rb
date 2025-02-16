class SrtExamples < Formula
  desc "Examples of SRT (Secure Reliable Transport) connection types"
  homepage "https://github.com/koensayr/srt_example"
  url "https://github.com/koensayr/srt_example/archive/v1.0.0.tar.gz"
  sha256 "REPLACE_WITH_ACTUAL_SHA256_AFTER_RELEASE"
  license "MIT"

  depends_on "cmake" => :build
  depends_on "srt"

  def install
    system "cmake", "-S", ".", "-B", "build", *std_cmake_args
    system "cmake", "--build", "build"
    
    # Install C++ executable
    bin.install "build/srt_example"
    
    # Install Python example to share directory
    share.install "srt_examples.py"
    
    # Create a symlink for the Python script in bin
    bin.write_exec_script share/"srt_examples.py"
  end

  test do
    # Test C++ executable
    assert_match "Modes:", shell_output("#{bin}/srt_example --help")
    
    # Test Python script
    assert_match "Modes:", shell_output("#{bin}/srt_examples.py --help")
  end
end
