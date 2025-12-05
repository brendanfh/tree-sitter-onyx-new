import XCTest
import SwiftTreeSitter
import TreeSitterOnyx

final class TreeSitterOnyxTests: XCTestCase {
    func testCanLoadGrammar() throws {
        let parser = Parser()
        let language = Language(language: tree_sitter_onyx())
        XCTAssertNoThrow(try parser.setLanguage(language),
                         "Error loading Onyx grammar")
    }
}
