import SwiftUI

struct ContentView: View {
    @State private var text = ""

    var body: some View {
        NavigationStack {
            TextEditor(text: $text)
                .font(.system(.body, design: .monospaced))
                .padding()
                .navigationTitle("Math Notes")
        }
    }
}
